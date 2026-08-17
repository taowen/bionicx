# XFCE panel/Thunar abort on Adwaita SVG

Untraced `xfce4-panel` and Thunar aborted:

```text
Gtk:ERROR:gtkiconhelper.c:495:ensure_surface_for_gicon
Failed to load .../Adwaita/scalable/status/image-missing.svg
Unable to load image-loading module:
/usr/lib/aarch64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader_svg.so
cannot open shared object file
```

`access()` on that FHS path succeeded. `dlopen` of the same path
returned ENOENT. Debian trixie `libgmodule-2.0.so.0` imports
`dlopen@GLIBC_2.34`. The runtime only exported `dlopen@GLIBC_2.17`, so
the versioned lookup skipped the FHS interpose and opened the Android
path.

## Controlled client

`examples/gdk-pixbuf-svg-probe` `access`es the loader, `dlopen`s it,
then `gdk_pixbuf_new_from_file` on the same Adwaita icon. Before the
second versioned export:

```text
BXTEST FAIL svg-loader-dlopen .../libpixbufloader_svg.so: No such file
BXSUMMARY gdk-pixbuf-svg passed=2 failed=2
```

GNU ld emits only one version when the same name is listed in two
version-script nodes. `.symver` aliases export both
`dlopen@GLIBC_2.17` and `dlopen@GLIBC_2.34`. After rebuild:

```text
BXSUMMARY gdk-pixbuf-svg passed=4 failed=0
BXSUMMARY gtk-icon-theme passed=7 failed=0
BXSUMMARY xfce-session-accept passed=11 failed=0
```
