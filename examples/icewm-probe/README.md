# IceWM desktop probe

This test runs the unmodified Debian ARM64 IceWM 3.7.4 binary as a genuine
glibc X11 window manager. IceWM selects `SubstructureRedirect` on the root
window, reparents two independently connected glibc clients into managed
frames, configures them and maps them.

The probe also enables IceWM's taskbar and verifies its mapped, screen-width
`TaskBar` window in the real X11 hierarchy. The bundle explicitly declares the
Imlib2 PNG and XPM plugins as runtime-loaded ELF entry points, so their own
dependency closures are resolved even though they are invisible from IceWM's
`DT_NEEDED` graph. This remains a deliberately small desktop boundary: it does
not introduce a file manager, D-Bus, login manager or package-management
redesign.

```sh
ANDROID_SERIAL=<serial> examples/icewm-probe/install-and-run.sh
```
