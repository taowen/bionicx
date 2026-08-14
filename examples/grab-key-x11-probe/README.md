# GrabKey probe

libX11 only. `XGrabKey` must accept `GrabModeSync` as well as
`GrabModeAsync`, including while the client holds `GrabServer`. This is
how xfsettingsd installs keyboard shortcuts at startup.

```sh
ANDROID_SERIAL=<serial> examples/grab-key-x11-probe/install-and-run.sh
```
