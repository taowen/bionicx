# WPS PDF `libtiff.so.5` on the shared rootfs

`wpspdf` `dlopen`s `office6/libpdfmain.so`, which needs bullseye
`libtiff.so.5`. Trixie only ships `libtiff6`. The controlled
`examples/wps-pdf-tiff-probe` first reproduced:

```text
BXTEST FAIL dlopen-libtiff5 libtiff.so.5: cannot open shared object file
BXTEST FAIL dlopen-pdfmain libtiff.so.5: cannot open shared object file
BXSUMMARY wps-pdf-tiff passed=1 failed=4
```

Hash-pinned `libwebp6` `0.6.1-2.1+deb11u2` and `libtiff5`
`4.2.0-1+deb11u5` from `packages/external-arm64.tsv` were installed with
`bxapt deb` into the shared rootfs. `dpkg --audit` stayed empty.
`files/apps` has no `libtiff.so.5`. The same probe is then 5/5:

```text
BXTEST PASS dlopen-libtiff5 .../usr/lib/aarch64-linux-gnu/libtiff.so.5
BXTEST PASS dlopen-pdfmain .../office6/libpdfmain.so
BXSUMMARY wps-pdf-tiff passed=5 failed=0
```

Untraced `profiles/wps-pdf.json` opens
`${APP}/fixtures/BionicX-PDF-Integration.pdf`. A fresh profile home shows
the Kingsoft EULA; the runner seeds `common\AcceptedEULA=true` (the same
flag Writer already stores). After Qt rasterizes, the compositor shows
page 1 (`BionicX PDF Page 1` / `glibc + X11 on Android`), thumbnails for
both pages, and `1/2`. The formula-symbol health dialog is still shown
and is not suppressed. `libproviders.so` is logged missing and is not
required for this page.

Rerun `examples/wps-pdf-tiff-probe/install-and-run.sh` to recapture under `build/evidence/`.
