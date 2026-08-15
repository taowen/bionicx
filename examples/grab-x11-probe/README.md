# Grab family probe

libX11 only. One client covers core `GrabKey` / `GrabKeyboard` /
`GrabPointer` / `GrabButton` Sync plus `AllowEvents` Async, Sync and
Replay, including under `GrabServer`.

`pointer-grab-x11-probe` and `keyboard-grab-x11-probe` stay separate:
those inject clicks and cover owner-events / replay routing.

```sh
ANDROID_SERIAL=<serial> examples/grab-x11-probe/install-and-run.sh
```
