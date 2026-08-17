# ReparentWindow probe

Two libX11 connections, no window manager binary. The manager connection
selects `SubstructureRedirect` on the root, answers `MapRequest` by
creating a frame, `ReparentWindow` into that frame, then mapping the
frame and the client. A mapped override-redirect window that is
reparented into an unmapped frame must emit `UnmapNotify` with
`from-configure` and become viewable only after the frame is mapped.

This is the xfwm4 `clientFrame` contract: reparent then map, including
`TYPE_DOCK` and `GrabServer`. XTEST then clicks the framed dock while
the frame also selects `ButtonPress`, and a synchronous `GrabButton` on
the frame `ReplayPointer`s through to the dock.

```sh
ANDROID_SERIAL=<serial> examples/reparent-x11-probe/install-and-run.sh
```
