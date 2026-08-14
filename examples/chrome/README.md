# Google Chrome stable ARM64

Chrome is a hash-pinned external Debian package installed by device-side
`bxapt` into the shared rootfs. Its GTK, NSS, MIME, icon and other dependencies
are ordinary entries in the same dpkg database; no Chrome rootfs or `/opt`
bundle exists.

The required `--no-sandbox` remains in both profiles. `chrome-smoke.json`
starts `about:blank` through ANGLE Vulkan / Vortek, then
`open-gpu.sh` types `chrome://gpu` into the omnibox (official Chrome
drops `chrome://` startup URLs). `chrome-vulkan.json` keeps the local
WebGL fixture. Both set `CHROME_EXTRA_FLAGS` so every Chrome process,
including `--type=` children, disables Crashpad and the GPU watchdog
before `ChromeMain`. `fhs-exec.c` does not rewrite Chrome argv.
`build-bundle.sh` builds only that host-driver bridge, ICD metadata and the
fixture, never Chrome or Debian libraries.

```sh
tools/bxapt --serial <serial> deb <google-chrome-stable_arm64.deb> <sha256>
examples/chrome/build-bundle.sh
BIONICX_CHROME_PROFILE=profiles/chrome-vulkan.json \
  ANDROID_SERIAL=<serial> examples/chrome/install-and-run.sh
```
