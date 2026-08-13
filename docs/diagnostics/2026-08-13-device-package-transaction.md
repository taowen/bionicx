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
4. let apt configure packages and run triggers;
5. normalize once more for generated artifacts.

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
