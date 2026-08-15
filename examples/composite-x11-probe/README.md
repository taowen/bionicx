# Composite family probe

libX11 only. One two-connection client covers Composite 0.2
`QueryVersion`, `RedirectWindow`, `NameWindowPixmap`/`GetImage`,
`UnredirectWindow` and `RedirectSubwindows`, including under
`GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/composite-x11-probe/install-and-run.sh
```
