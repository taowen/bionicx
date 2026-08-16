# NameWindowPixmap after resize probe

libX11 only. Two connections: the compositor names a redirected window
pixmap, the other client resizes and paints. The old pixmap must stay
frozen at the old size; a new `NameWindowPixmap` sees the new backing.
Does not start `xfwm4` or `xfsettingsd`.

```sh
ANDROID_SERIAL=<serial> examples/named-pixmap-resize-x11-probe/install-and-run.sh
```
