# GrabButton probe

libX11 only. `XGrabButton` must accept `GrabModeSync` as well as
`GrabModeAsync`, including while the client holds `GrabServer`. GDK
installs passive button grabs with a synchronous keyboard mode.

This is separate from `pointer-grab-x11-probe`, which covers owner-events
and replay routing with injected clicks and only uses keyboard-mode Async.

```sh
ANDROID_SERIAL=<serial> examples/grab-button-x11-probe/install-and-run.sh
```
