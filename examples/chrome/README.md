# Google Chrome stable ARM64

This is the first Chromium-class BionicX example. `build-bundle.sh` verifies
the pinned Google ARM64 package and gives application ELFs the app-private
glibc interpreter. Chrome, WPS and IceWM are installed by normal `apt`/`dpkg` into a
pinned Debian 13 rootfs, so NSS modules, GTK, GSettings schemas, MIME data,
icons, GDK Pixbuf plugins and other package-level dependencies are retained
without manually declaring hidden `dlopen()` roots. The generated Pixbuf cache
is relocated to the final app-private Android runtime path. Installing another
desktop profile does not retransmit the content-addressed rootfs.

```sh
examples/chrome/build-bundle.sh
ANDROID_SERIAL=<serial> examples/chrome/install-and-run.sh
```

The build executes package maintainer scripts and triggers in an isolated ARM64
Debian container, with service startup disabled. Proprietary binaries and
downloaded packages remain ignored beneath `build/`; only acquisition facts,
the package manifest, source, diagnostics and test evidence are committed.

The normal profile retains the required `--no-sandbox` and selects ANGLE's
OpenGL backend over the BionicX Gladio bridge. Chrome's Ganesh renderer then
runs on the Android host GLES driver; it is not a software-rendering fallback.

`profiles/chrome-vulkan.json` is a separate experimental launch profile. It
keeps `--no-sandbox`, selects ANGLE's Vulkan backend, and routes the ordinary
glibc Vulkan loader through the pinned Vortek ICD to Android's vendor driver.
Set `BIONICX_CHROME_PROFILE=profiles/chrome-vulkan.json` when running the
installer. Skia Graphite remains disabled so ANGLE Vulkan can be qualified
before changing Chrome's higher-level renderer.
