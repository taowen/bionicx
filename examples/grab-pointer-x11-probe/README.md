# GrabPointer probe

libX11 only. `XGrabPointer` must accept `GrabModeSync` and a nonzero
timestamp, including under `GrabServer`. GDK always passes the last
event time.

This is separate from `pointer-grab-x11-probe`, which covers owner-events
and replay routing with injected clicks.

```sh
ANDROID_SERIAL=<serial> examples/grab-pointer-x11-probe/install-and-run.sh
```
