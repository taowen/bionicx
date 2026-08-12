# Google Chrome stable ARM64

This is the first Chromium-class BionicX example. `build-bundle.sh` downloads
the pinned Google ARM64 package, verifies its SHA-256, resolves dependencies in
a native ARM64 Debian userspace, and rejects any package-set drift from
`dependencies.lock`. Application ELFs receive the app-private glibc
interpreter. The dependency closure includes explicit NSS runtime modules that
ordinary `DT_NEEDED` traversal cannot discover. It also declares GTK 3 as a
dynamic root because Chromium loads its Linux native UI with `dlopen()`; this
keeps native file dialogs in the same recursive, hash-recorded ELF closure.
GTK's architecture-independent GSettings XML is compiled into the private app
tree so Chrome does not require a host `/usr/share` or dconf session service.
The same private data root contains a generated shared-MIME database and cached
Adwaita/hicolor icon themes required by GTK's native file chooser. Runtime-
loaded GDK Pixbuf plugins are explicit dependency roots, and their ARM64-
generated loader cache is rewritten to the final app-private Android paths.

```sh
examples/chrome/build-bundle.sh
ANDROID_SERIAL=<serial> examples/chrome/install-and-run.sh
```

The build never executes Chrome or its maintainer scripts. Debian packages are
data inputs extracted into a temporary staging tree. Proprietary binaries and
downloaded packages remain ignored beneath `build/`; only acquisition facts,
the dependency lock, source, diagnostics, and test evidence are committed.

The current profile retains the required `--no-sandbox` and selects ANGLE's
OpenGL backend over the BionicX Gladio bridge. Chrome's Ganesh renderer then
runs on the Android host GLES driver; it is not a software-rendering fallback.
Skia Graphite remains disabled until the native Vulkan bridge is implemented.
The Chromium sandbox and Vulkan backend are explicit remaining qualification
gaps, not properties hidden by the installer.
