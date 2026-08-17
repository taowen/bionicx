# GTK icon-theme probe

Thunar's toolbar buttons stay empty when `GtkIconTheme` cannot load
Adwaita action/place icons. This client is that path without XFCE:
`dlopen` Debian `libgtk-3.so.0`, then `gtk_icon_theme_load_icon` for
the same names (`go-previous-symbolic`, `folder`).

```sh
ANDROID_SERIAL=<serial> examples/gtk-icon-theme-probe/install-and-run.sh
```
