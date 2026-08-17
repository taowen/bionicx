# GTK desktop-icon probe

xfdesktop Home / File System and the panel Applications button need
`user-home`, `drive-harddisk`, and `org.xfce.panel.applicationsmenu`.
This client is that path without XFCE: `dlopen` Debian `libgtk-3.so.0`,
force Adwaita, then `gtk_icon_theme_load_icon` and check the pixbuf
colors (blue house, cyan mouse).

```sh
ANDROID_SERIAL=<serial> examples/gtk-desktop-icon-probe/install-and-run.sh
```
