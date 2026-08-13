# WPS Office ARM64

WPS is installed into the shared Debian 13 rootfs by the real device-side
`dpkg`/`apt`, not copied into a profile bundle. The repository does not ship
WPS and its license remains the user's responsibility.

```sh
tools/bxapt --serial <serial> install xdg-utils
tools/bxapt --serial <serial> deb \
  build/cache/wps-11.1.0.11720/wps-office_11.1.0.11720_arm64.deb \
  172d8bcf3e3bc756994ae5adf66f63f22691e1ab0d18986c50bb6b6ab7f62948
tools/install-profile.sh --serial <serial> --profile profiles/wps-office.json
```

The upstream WPS package calls `xdg-icon-resource` from a non-idempotent
`postinst` but does not declare `xdg-utils`. Install the Debian provider first:
if configuration fails after the package has renamed its desktop files, a
plain configure retry also fails. Re-running the same hash-pinned `bxapt deb`
transaction restores the package-owned files by unpacking before it configures.

`bxapt` resolves dependencies in the one dpkg database, then applies the same
ELF normalization as every Debian package. Writer, Spreadsheets, Presentation
and PDF are package-owned entrypoints under `${RUNTIME}/opt/kingsoft`; none has
a private dependency closure, loader mode, application preload, or fallback.
On a thin seed the Qt xcb plugin also needs `libxslt1.1` plus
`libxkbcommon-x11-0` and the `libxcb-icccm4` / `image0` / `keysyms1` /
`render-util0` / `xinerama0` / `xkb1` / `util1` set. `wpspdf` still needs
`libtiff.so.5`, which trixie does not ship.

Android seccomp blocks the ARM64 System V semaphore syscalls used by WPS. The
mandatory runtime therefore provides one cross-process app-private semaphore
namespace, also covered by the generic runtime integration probe. It is not
selected by the WPS profile.

`build-bundle.sh` only creates deterministic document fixtures. The OOXML/PDF
verification scripts inspect files saved in the profile home and remain useful
for real feature acceptance without copying proprietary binaries to the host.
