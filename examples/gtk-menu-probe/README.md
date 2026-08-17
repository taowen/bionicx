# GTK menu popup probe

The XFCE Applications button is a GTK menu: `gtk_menu_popup_at_widget`
maps an override-redirect `GDK_WINDOW_TEMP`. This client is that path
without XFCE or `xfwm4`.

```sh
ANDROID_SERIAL=<serial> examples/gtk-menu-probe/install-and-run.sh
```
