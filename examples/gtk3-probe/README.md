# GTK3 dynamic native-dialog probe

This genuine AArch64 glibc client isolates the Linux desktop-toolkit path used
by Chromium. It loads `libgtk-3.so.0` by its runtime SONAME, initializes GTK on
the existing X11 display, maps a normal window, and creates a real
`GtkFileChooserDialog`, with a machine-readable result after each boundary.

The probe is intentionally usable inside a larger application's installed
runtime closure. That catches missing `dlopen()` roots and GTK modules without
instrumenting or rebuilding the application itself.

It tests PNG twice: once through automatic file-format detection and once by
constructing a loader explicitly with type `png`. This distinguishes signature
matching failures from failures in the PNG decoder itself.

It also resolves a real `sans-serif` face through Pango/Fontconfig and maps a
GTK label before opening the file chooser. This separates missing font data
from an XRender or window-composition defect when text is absent on screen.

```sh
examples/gtk3-probe/build.sh
```
