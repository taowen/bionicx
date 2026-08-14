# XI2 AllowEvents probe

libXi only. `XIAllowEvents` must accept `XIAsyncDevice`, `XIReplayDevice`,
`XISyncDevice` and a nonzero timestamp, including under `GrabServer`.
GDK thaws after `XIGrabModeSync` this way.

```sh
ANDROID_SERIAL=<serial> examples/xi2-allow-events-x11-probe/install-and-run.sh
```
