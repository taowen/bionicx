# Device package transaction boundary

## Failure exposed by the clean seed

Installing Debian `dbus`, `icewm` and `xterm` directly from the fixed snapshot
unpacked 124 packages, then attempted their maintainer scripts before the new
ELFs had entered BionicX's fixed loader/RUNPATH contract. Helpers such as
`dbus-uuidgen`, `glib-compile-schemas` and `update-mime-database` consequently
failed. Normalizing only after a successful apt command was too late.

The same transaction exposed three rootfs-wide assumptions:

- Fontconfig needs its standard `FONTCONFIG_SYSROOT`, not per-profile copies of
  `FONTCONFIG_PATH`/`FONTCONFIG_FILE` configuration;
- Debian maintainer scripts use `chroot $DPKG_ROOT`; inside that virtual root
  logical uid/gid 0 is required, while Android kernel credentials must remain
  the app UID;
- Debian system users cannot be represented as Android file owners. The
  official `systemd-standalone-sysusers --root` updates rootfs account files,
  while unsupported ownership metadata is intentionally not materialized.

## Unified transaction

`bxapt install` and `bxapt set` now use one deterministic sequence:

1. resolve and download with the rootfs's signed apt;
2. unpack the resolved `.deb` set with the same dpkg database;
3. normalize all newly installed dynamic ELFs;
4. let apt configure packages and run triggers.

The original implementation implemented steps 3 and 4 with apt hooks that
scanned the entire rootfs twice. Once WPS and the popular application cohort
made that rootfs roughly 5 GB, even a small font package spent minutes walking
unrelated `/opt` payloads. The unpack stage now records every path in the exact
downloaded deb set. The provider index remains rootfs-wide, while normalization
and atomic ledger replacement are limited to changed package paths. Package
configuration no longer repeats the scan. An explicit `bxapt fixup` still
performs a complete audit when requested.

External `.deb` installation uses the same unpack-normalize-configure order.
There is no package-specific dependency copy, alternate loader path or failed
transaction fallback. `systemd-standalone-sysusers` is part of the minimal seed
because it is package-manager infrastructure for a non-systemd rootfs, not an
IceWM or D-Bus payload dependency.

Fontconfig settings and `/run` mapping are runtime-owned and identical for all
profiles and package helpers. The repeated Fontconfig variables were removed
from every application profile.

## Device result

On `01408BH601027129`, the recovered clean-seed transaction ended with an empty
`dpkg --audit` and these exact installed states:

```text
dbus                            1.16.2-2                 ii
icewm                           3.7.4-1                  ii
systemd-standalone-sysusers     257.13-1~deb13u1         ii
xterm                           398-1                    ii
```

The package-installed xterm then started Debian bash from that same rootfs.
Android input executed `echo bionicx-shared-rootfs`, and both command and output
were rendered by the real X11 client. The screenshot is
`evidence/rebuild-2026-08-13/xterm-shared-rootfs-command.png`.

The package-installed IceWM 3.7.4 was then launched from the same rootfs with
two independent controlled glibc X11 clients. It selected the root redirect,
reparented and decorated both clients, painted both title bars and close
buttons, mapped its full-width taskbar and retained both clients for their
complete lifetime. The untraced result was `4/4`, with the live desktop in
`evidence/rebuild-2026-08-13/icewm-shared-rootfs.png`.

The complete original failure, staged repair and final audit are retained in
`evidence/rebuild-2026-08-13/`.

## Incremental transaction result

The device installed the data-only `fonts-stix` and `fonts-opensymbol` packages
from the pinned snapshot in 8.28 seconds, including a five-second download,
with an empty dpkg audit. A stronger test then reinstalled Debian's real ARM64
`xterm` package: the transaction finished in 5.53 seconds, the unpacked xterm
was rewritten to the app-private glibc interpreter, dpkg audit remained empty,
and the reinstalled binary rendered a live bash prompt through the Android X11
server. The normalizer still enumerated rootfs shared-library names for its
global provider index, but it did not traverse or inspect unrelated executable
ELFs and did not enter the WPS payload as a rewrite target.

The first `--reinstall` trial also caught a configuration-boundary bug: passing
that download option into the final configure-only apt invocation made apt ask
for an archive already consumed by the manual unpack stage. The final stage is
now always the one consistent `apt-get -f install` configuration operation.
The interrupted xterm state was configured, audited, and then the complete
reinstall was repeated successfully; it is not a fallback execution mode.

The hash-pinned external-deb path was separately exercised with Debian's ARM64
`libatomic1` package and finished in 3.75 seconds. That test caught the POSIX
awk empty-first-file edge in atomic ledger merging when apt resolved no extra
dependencies. The installed library and dpkg state were already correct, but
the empty second manifest had cleared the audit ledger. Empty manifests now
copy the previous ledger verbatim and have a host regression. One explicit
full audit restored the device ledger to 1,300 entries; a subsequent empty
device transaction preserved its SHA-256 exactly and `dpkg --audit` stayed
empty.

## Native apt state after an explicit unpack boundary

An attempted simplification used apt's `Pre-Install-Pkgs` to collect the same
manifest and `DPkg::Post-Invoke` to normalize it. Trixie's official hook
protocol only supplies the archive list; it cannot rewrite apt's package plan.
The device transaction also showed that `Post-Invoke` ran after xterm had
already been configured, which is too late for maintainer-script helpers. That
design was rejected rather than retained as another mode.

The explicit dpkg unpack boundary is therefore necessary, but it means apt did
not get a chance to write automatic/manual dependency marks. BionicX now
snapshots installed package names before resolution and reconciles the
successful transaction delta with Debian's own `apt-mark`: new dependencies
are automatic, while packages named by `install`, `set`, or the hash-pinned
external deb are manual. A controlled `jq` transaction installed new `libjq1`
and `libonig5`; the two libraries appeared in `apt-mark showauto` and only jq
appeared in `showmanual`. Removing jq made exactly those two libraries eligible
for autoremove, and the real apt autoremove deleted them cleanly.

The temporary `libcups2-dev` experiment and all 32 dependencies that it had
introduced were removed after the CUPS hypothesis was disproved, reclaiming
94.4 MB. Incremental removal now prunes nonexistent paths from the ELF ledger;
the cleanup reduced it from 1,306 stale-inclusive records to 1,237 live
records while leaving `dpkg --audit` empty.
