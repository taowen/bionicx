# GTK menu popup probe

The XFCE Applications button is a GTK menu. This client is that path
without XFCE or `xfwm4`: a second connection holds `SubstructureRedirect`,
reparents the host into a frame that selects `ButtonPress` (xfwm4 dock),
`gtk_menu_popup_at_widget` maps a sized `GDK_WINDOW_TEMP` whose interior
is not an unpainted black pixmap, a 16-item menu keeps a light
background even when `_NET_WM_CM_S0` is owned and the menu was realized
before popup, a `GarconGtkMenu` from `xfce-applications.menu` stays
light, XTEST clicks a `GtkMenuButton`, a
`button-press-event` handler (the Applications plugin path) pops a menu
when button 1 is pressed with no Control, and the same press still pops
a menu when the stub WM holds a synchronous `GrabButton` on the client
and XI2 on the frame, then `ReplayPointer`s.

```sh
ANDROID_SERIAL=<serial> examples/gtk-menu-probe/install-and-run.sh
```
