# IceWM desktop probe

This test runs the unmodified Debian ARM64 IceWM 3.7.4 binary as a genuine
glibc X11 window manager. IceWM selects `SubstructureRedirect` on the root
window, reparents two independently connected glibc clients into managed
frames, configures them and maps them.

The probe recursively inspects the live hierarchy and reads back the server-side
pixels for both title bars and close buttons. This catches cases where frame
geometry exists but missing visibility events leave its decorations black.

The probe also enables IceWM's taskbar and verifies its mapped, screen-width
`TaskBar` window in the real X11 hierarchy. apt retains Imlib2's runtime-loaded
PNG and XPM plugins and their package dependencies even though they are
invisible from IceWM's `DT_NEEDED` graph. IceWM remains the first deliberately
small window-manager boundary; the shared system image does not imply that a
login manager or systemd is started on Android.

IceWM is installed by apt into the same pinned Debian 13 rootfs as Chrome and
WPS. Its package-owned libraries, Imlib2 loaders and shared artwork keep their
native multiarch/FHS layout. The probe application layer contains only the WM
entrypoint, controlled clients and private configuration.

```sh
ANDROID_SERIAL=<serial> examples/icewm-probe/install-and-run.sh
```
