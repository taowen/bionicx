# Package-installed trixie desktop rootfs

## Decision

BionicX fixes its Linux userspace baseline to Debian 13 (trixie), snapshot
`20260811T000000Z`, on ARM64. The OCI base is pinned by digest and the Chrome
and WPS packages are pinned by version and SHA-256. Chrome, WPS and IceWM are
installed together with ordinary apt/dpkg in an isolated ARM64 Debian build
container. Service startup is disabled during package configuration.

The resulting `/etc`, `/usr`, `/var`, merged-usr links and `/var/lib/dpkg`
become one immutable Android runtime. Chrome's `/opt/google/chrome` and WPS's
`/opt/kingsoft` are split into application payloads after configuration. apt,
dpkg and maintainer scripts never run on Android. This supersedes the
experimental per-application flat ELF closures: hidden `dlopen` dependencies,
plugins, schemas, icons and package data now follow Debian package semantics.

The current image contains 275 packages, including six explicit/manual roots:
Chrome, WPS, IceWM, `bsdextrautils`, `libxslt1.1`, and
`libxkbcommon-x11-0`. It occupies approximately 519 MiB; the split Chrome and
WPS payloads occupy approximately 425 MiB and 1.5 GiB respectively. The
content ID deployed on x300 is
`17ee253ec8a996df1aa5048e24aa7f25eb5f547b77a5e77ee1913daf6d3aeeb2`.
All 1069 ELF objects in the system layer require at most `GLIBC_2.41`.

## WPS metadata gaps found by the migration

Normal package installation exposed three omissions in WPS Office
11.1.0.11720's Debian metadata:

1. Its postinst invokes `hexdump` without depending on a provider. The build
   installs `bsdextrautils` explicitly.
2. `libwpsmain.so` loads `libxslt.so.1` without declaring `libxslt1.1`.
3. Its bundled Qt xcb plugin links `libxkbcommon-x11.so.0` without declaring
   `libxkbcommon-x11-0`; apt also installs `libxcb-xkb1` transitively.

These remain package-level workarounds. No individual Debian shared object is
copied into an application bundle. Recursive ELF auditing of `libqxcb.so`
reports an empty missing set after the third package is installed.

WPS's postinst also changes its private `libstdc++.so.6` link to an absolute
`/usr/lib/aarch64-linux-gnu/...` target. BionicX is deliberately not a chroot
or PRoot, so the WPS bundle removes that package-generated link and resolves
the same Debian system library through the shared multiarch `LD_LIBRARY_PATH`.

## Device validation

All final launches below ran untraced under Android's ordinary application UID
on x300 (`01408BH601027129`), with no PRoot, Termux, Frida or root in the
runtime path.

- IceWM retained its complete taskbar, two managed clients, title bars and
  controls; `icewm-probe` passed 4/4.
- Chrome 151.0.7922.108 reached a complete 1920x1080 `about:blank` UI from the
  package-installed rootfs while retaining `--no-sandbox`; see
  `evidence/bionicx-trixie-apt-chrome.png`.
- WPS Writer reached its real home screen, created `Document1`, and accepted
  `BionicX_WPS_trixie_2026` in the editor; see
  `evidence/bionicx-trixie-apt-wps-edit.png`.

The rootfs now supplies a deterministic non-empty machine ID. D-Bus packages
are installed as dependencies but no session/system bus is started yet; that
is a separate desktop-service capability rather than a reason to fork another
application-specific dependency tree.

## Deployment invariant

The rootfs manifest hashes every regular file and produces a content ID.
`tools/install-profile.sh` reuses an identical device rootfs and clears an old
one before installing a different ID, preventing stale libraries from hiding
an incomplete build. Host application bundles hard-link the canonical rootfs
instead of multiplying its storage.
