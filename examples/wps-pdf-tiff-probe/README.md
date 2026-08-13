# WPS PDF `libtiff.so.5` probe

`wpspdf` `dlopen`s `office6/libpdfmain.so`, which needs bullseye
`libtiff.so.5`. Trixie only ships `libtiff6`. The probe installs the
hash-pinned `libwebp6` and `libtiff5` debs from `packages/external-arm64.tsv`
through `bxapt deb` into the shared rootfs, then `dlopen`s both SONAMEs.
Expect `BXSUMMARY wps-pdf-tiff passed=5 failed=0`. The libraries must not
appear under `files/apps`.

```sh
ANDROID_SERIAL=<serial> examples/wps-pdf-tiff-probe/install-and-run.sh
```
