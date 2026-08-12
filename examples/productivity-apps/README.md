# Firefox ESR, LibreOffice Writer, Evince, GIMP and Inkscape

This cohort takes all dependencies from the fixed Debian 13 trixie ARM64
snapshot and installs them into the same apt/dpkg rootfs as the existing apps.
It adds no per-application shared-library copies.

```sh
examples/productivity-apps/build-bundle.sh
tools/install-profile.sh --profile profiles/firefox-esr.json \
  --app-root build/productivity-apps-bundle/app \
  --runtime-root build/productivity-apps-bundle/rootfs
```

The deterministic fixtures cover browser rendering/network navigation, ODF
editing/save/reopen, two-page PDF rendering/navigation, raster image editing,
and vector artwork editing/export. Merely reaching a splash screen does not
count as acceptance.
