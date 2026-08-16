# Typed XSETTINGS payload probe

libX11 only. Two connections follow the xfsettingsd → GTK path without
starting either: the manager publishes typed `_XSETTINGS_SETTINGS`
(`Xft/DPI`, `Gtk/FontName`), the client parses them, sees
`PropertyNotify` on the manager window, and re-reads after an update
under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/xsettings-typed-x11-probe/install-and-run.sh
```
