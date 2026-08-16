# xfwm4-shaped compositor probe

libX11 only. Two connections follow the xfwm4 compositor order:
`GetOverlayWindow` (overlay stays unmapped until this client maps it),
`RedirectSubwindows(root)`, a mapped toplevel, overlay stays top after
`LowerWindow`, overlay `SetWindowShapeRegion` Bounding/Clip/Input,
`DamageNotify`, `NameWindowPixmap` readback, and paint to overlay.
Does not start `xfwm4` or `xfsettingsd`.

```sh
ANDROID_SERIAL=<serial> examples/compositor-x11-probe/install-and-run.sh
```
