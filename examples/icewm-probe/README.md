# IceWM desktop probe

This test runs the unmodified Debian ARM64 IceWM 3.7.4 binary as a genuine
glibc X11 window manager. IceWM selects `SubstructureRedirect` on the root
window, reparents two independently connected glibc clients into managed
frames, configures them and maps them.

The first milestone intentionally disables IceWM's taskbar. It validates the
smallest useful multi-window desktop boundary without introducing a file
manager, D-Bus, login manager or a package-management redesign.

```sh
ANDROID_SERIAL=<serial> examples/icewm-probe/install-and-run.sh
```
