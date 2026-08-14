# MapRequest / SubstructureRedirect probe

Two libX11 connections, no window manager binary. The manager connection
selects `SubstructureRedirect` on the root. The client maps a normal window
and a `_NET_WM_WINDOW_TYPE_DOCK` window. Both must stay unmapped until the
manager answers `MapRequest`. A `GrabServer` owner must still be able to map.

This is the XFCE panel failure: a dock `MapWindow` under a redirecting WM.

```sh
ANDROID_SERIAL=<serial> examples/map-request-x11-probe/install-and-run.sh
```
