# Root-redirect resize screen probe

libX11 only. Two connections follow the xfwm4 compositor order:
`GetOverlayWindow`, `RedirectSubwindows(root)`, a filled toplevel, then
`ConfigureWindow` and a second fill. `GetImage` must see the new pixels.
The install script also asserts the after-resize color on an Android
screenshot. Does not start `xfwm4` or `xfsettingsd`.

```sh
ANDROID_SERIAL=<serial> examples/resize-screen-x11-probe/install-and-run.sh
```
