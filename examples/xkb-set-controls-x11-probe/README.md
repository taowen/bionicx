# XKB SetControls probe

libX11 only. `XkbSetAutoRepeatRate` (`XkbSetControls`) must accept a
new delay/interval and `XkbGetAutoRepeatRate` must echo it, including
under `GrabServer`. This is how xfsettingsd applies keyboard repeat.

```sh
ANDROID_SERIAL=<serial> examples/xkb-set-controls-x11-probe/install-and-run.sh
```
