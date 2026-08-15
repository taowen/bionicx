# XI family probe

libXi only. One client covers XI1 `ListInputDevices` /
`SelectExtensionEvent`, XI2 `XIGrabDevice` Sync, `XIAllowEvents`, and
`XIGrabButton` passive grabs, including under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/xi-x11-probe/install-and-run.sh
```
