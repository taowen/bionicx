# gdk-pixbuf SVG loader probe

xfce4-panel and Thunar abort in `gtkiconhelper` when Adwaita
`image-missing.svg` cannot load. This client is that path without XFCE:
`dlopen` the FHS SVG pixbuf module, then `gdk_pixbuf_new_from_file` on
the same Adwaita icon.

```sh
ANDROID_SERIAL=<serial> examples/gdk-pixbuf-svg-probe/install-and-run.sh
```
