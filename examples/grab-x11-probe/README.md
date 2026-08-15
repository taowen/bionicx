# Grab family probe

libX11 only. One two-connection client covers core `GrabKey` /
`GrabKeyboard` / `GrabPointer` / `GrabButton` Sync, `AllowEvents`, and
`GrabPointer`/`GrabButton` `confineTo` clamping, including under
`GrabServer`.

`pointer-grab-x11-probe` and `keyboard-grab-x11-probe` stay separate:
those inject clicks and cover owner-events / replay routing.

```sh
ANDROID_SERIAL=<serial> examples/grab-x11-probe/install-and-run.sh
```
