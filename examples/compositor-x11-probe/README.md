# xfwm4-shaped compositor probe

libX11 only. Two connections follow the xfwm4 compositor order:
`GetOverlayWindow` (overlay stays unmapped until this client maps it),
`RedirectSubwindows(root)`, a mapped toplevel, overlay stays top after
`LowerWindow`, overlay `SetWindowShapeRegion` Bounding/Clip/Input,
`DamageNotify`, `NameWindowPixmap` readback, paint to overlay, an
xfwm4-style child output window on the overlay, the GLX
`ClientInfo` / `SetClientInfoARB` / `SetClientInfo2ARB` handshake,
and a paint burst of 8-bit A8 / 1x1 repeat / `SetPictureClipRegion` /
`Composite` plus a large A8 `PutImage` immediately after `UngrabServer`,
then `CreatePicture` on the overlay child and a named pixmap to present
into that output. Does not start `xfwm4` or `xfsettingsd`.

```sh
ANDROID_SERIAL=<serial> examples/compositor-x11-probe/install-and-run.sh
```
