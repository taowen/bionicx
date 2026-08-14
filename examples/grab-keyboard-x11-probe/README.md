# GrabKeyboard probe

libX11 only. `XGrabKeyboard` must accept `GrabModeSync` and a nonzero
timestamp, and `XUngrabKeyboard` must accept a nonzero timestamp,
including under `GrabServer`. GDK always passes the last event time.

```sh
ANDROID_SERIAL=<serial> examples/grab-keyboard-x11-probe/install-and-run.sh
```
