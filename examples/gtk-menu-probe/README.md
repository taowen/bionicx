# GTK menu popup probe

The XFCE Applications button is a GTK menu. This client is that path
without XFCE or `xfwm4`: a second connection holds `SubstructureRedirect`,
reparents the host into a frame that selects `ButtonPress` (xfwm4 dock),
`gtk_menu_popup_at_widget` maps a sized `GDK_WINDOW_TEMP` whose interior
is not an unpainted black pixmap, XTEST clicks a `GtkMenuButton`, and a
`button-press-event` handler (the Applications plugin path) pops a menu
when button 1 is pressed with no Control.

```sh
ANDROID_SERIAL=<serial> examples/gtk-menu-probe/install-and-run.sh
```
