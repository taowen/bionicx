# XKB LatchLockState probe

libX11 only. `XkbLockModifiers` / `XkbLatchModifiers` must update
`XkbGetState`, including under `GrabServer`. GTK uses this for Caps
and Num lock.

```sh
ANDROID_SERIAL=<serial> examples/xkb-latch-lock-x11-probe/install-and-run.sh
```
