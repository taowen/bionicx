# Composite path probe

libX11 only. Two connections (compositor + client) cover
`RedirectSubwindows` offscreen backing, `DamageNotify` from `PutImage`,
`NameWindowPixmap` readback, and painting those pixels onto the Composite
overlay. Does not start `xfwm4` or `xfsettingsd`.

```sh
ANDROID_SERIAL=<serial> examples/composite-path-x11-probe/install-and-run.sh
```
