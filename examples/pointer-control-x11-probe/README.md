# Pointer control probe

libX11 only. `XChangePointerControl` / `XGetPointerControl` must store
and echo acceleration and threshold, including a partial update and
under `GrabServer`. This is how xfsettingsd applies mouse speed.

```sh
ANDROID_SERIAL=<serial> examples/pointer-control-x11-probe/install-and-run.sh
```
