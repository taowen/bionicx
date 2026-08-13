# GTK composite-template resource probe

Genuine AArch64 glibc client that `dlopen`s Debian `libgtk-3.so.0`,
initializes GTK, and looks up `/org/gtk/libgtk/ui/gtkstatusbar.ui`.
Mousepad crashes when that compiled-in GResource is missing.

The payload is installed against the existing seed rootfs.

```sh
ANDROID_SERIAL=<serial> examples/gtk-template-probe/install-and-run.sh
```
