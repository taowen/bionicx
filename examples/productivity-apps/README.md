# Firefox ESR, LibreOffice Writer, Evince, GIMP and Inkscape

This cohort takes all dependencies from the fixed Debian 13 trixie ARM64
snapshot and installs them into the same apt/dpkg rootfs as the existing apps.
It adds no per-application shared-library copies.

```sh
examples/productivity-apps/build-bundle.sh
ANDROID_SERIAL=<serial> examples/popular-apps/install-and-run.sh firefox-esr-online
```

Install with `--app-root` only. The fixture bundle has no Debian rootfs
and must not replace the shared device seed.

The deterministic fixtures cover browser rendering/network navigation, ODF
editing/save/reopen, two-page PDF rendering/navigation, raster image editing,
and vector artwork editing/export. Merely reaching a splash screen does not
count as acceptance.
