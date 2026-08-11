# WPS PDF entrypoint and legacy image dependencies

## First direct launch

The installed WPS package contains a genuine AArch64 `office6/wpspdf` ELF, and
its desktop file launches it directly. Its original interpreter was
`/lib/ld-linux-aarch64.so.1`; `examples/wps/install-entrypoints.sh` now relocates
it to the same app-private glibc loader used by Writer, Spreadsheets, and
Presentation.

The first untraced `profiles/wps-pdf.json` run on x300
`01408BH601027129`, Android API 34, exited before creating a window:

```text
dlopen .../libpdfmain.so failed, error: libtiff.so.5: cannot open shared object file
wps-office exited with 255
```

This is an ELF dependency failure rather than an X11 rendering failure.

## Reproducible dependency closure

The inspected Pi-Apps `install-64` baseline explicitly installs `libwebp6` and
`libtiff5` for WPS PDF export, falling back to exact Debian ARM64 packages. The
TIFF ELF additionally needs `libjpeg.so.62`, `libdeflate.so.0`, and
`libjbig.so.0`, which were absent from the compact BionicX runtime.

`examples/wps/install-pdf-libs.sh` downloads the baseline packages and those
two transitive dependencies, verifies five pinned SHA-256 hashes, extracts only
the needed runtime libraries, verifies AArch64 ELF identity, installs them into
the app-private glibc `usr/lib`, and compares every device regular-file hash.
It runs no package manager or root command on Android.

## Result and boundary

After installation, the same formal profile records
`bionicx-exec: running untraced` and reaches the full-screen WPS PDF home UI.
There is no missing library, unsupported X request, abnormal process exit,
PRoot, Termux, Frida, or root runtime.

This checkpoint proves dependency closure and the real PDF entrypoint only.
Opening, rendering, zooming, and navigating a controlled PDF remain the next
functional check. See `evidence/wps-pdf-entrypoint.log` and
`evidence/wps-pdf-home.png`.
