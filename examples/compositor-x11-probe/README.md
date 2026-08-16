# xfwm4-shaped compositor probe

libX11 only. Two connections follow the xfwm4 compositor order:
`GetOverlayWindow` (overlay stays unmapped until this client maps it),
`RedirectSubwindows(root)`, a mapped toplevel, overlay stays top after
`LowerWindow`, overlay `SetWindowShapeRegion` Bounding/Clip/Input,
`DamageNotify`, `NameWindowPixmap` readback, paint to overlay, an
xfwm4-style child output window on the overlay, and the GLX
`ClientInfo` / `SetClientInfoARB` / `SetClientInfo2ARB` handshake
xfwm4 sends while probing GLX. Does not start `xfwm4` or `xfsettingsd`.

```sh
ANDROID_SERIAL=<serial> examples/compositor-x11-probe/install-and-run.sh
```
