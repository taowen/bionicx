# XI2 passive grab probe

libXi only. `XIGrabButton` (`XIPassiveGrabDevice`) must accept
`XIGrabModeSync` as well as Async, including under `GrabServer`.
GDK installs passive button grabs this way.

```sh
ANDROID_SERIAL=<serial> examples/xi2-passive-grab-x11-probe/install-and-run.sh
```
