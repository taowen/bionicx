# XI2 GrabDevice probe

libXi only. `XIGrabDevice` must accept `XIGrabModeSync` and a nonzero
timestamp, including under `GrabServer`. GDK always passes the last
event time.

This is separate from `x11-desktop-probe`, which covers owner-events
and live DeviceEvents with `CurrentTime` + Async only.

```sh
ANDROID_SERIAL=<serial> examples/xi2-grab-x11-probe/install-and-run.sh
```
