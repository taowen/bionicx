# GTK menu popup probe

The XFCE Applications button is a GTK menu. This client is that path
without XFCE or `xfwm4`: a second connection holds `SubstructureRedirect`
and answers `MapRequest`/`ConfigureRequest`, `gtk_menu_popup_at_widget`
maps a sized `GDK_WINDOW_TEMP`, and XTEST clicks a `GtkMenuButton`.

```sh
ANDROID_SERIAL=<serial> examples/gtk-menu-probe/install-and-run.sh
```
