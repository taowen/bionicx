# Google Chrome stable ARM64

Chrome is a hash-pinned external Debian package installed by device-side
`bxapt` into the shared rootfs. Its GTK, NSS, MIME, icon and other dependencies
are ordinary entries in the same dpkg database; no Chrome rootfs or `/opt`
bundle exists.

The required `--no-sandbox` remains in both profiles. `chrome-smoke.json` uses
the host GLES path through Gladio. `chrome-vulkan.json` uses the Vortek bridge;
`build-bundle.sh` builds only that host-driver bridge and ICD metadata, never
Chrome or Debian libraries.

```sh
tools/bxapt --serial <serial> deb <google-chrome-stable_arm64.deb> <sha256>
examples/chrome/build-bundle.sh
BIONICX_CHROME_PROFILE=profiles/chrome-vulkan.json \
  ANDROID_SERIAL=<serial> examples/chrome/install-and-run.sh
```
