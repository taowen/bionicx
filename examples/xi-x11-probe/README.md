# XI family probe

libXi only. One two-connection client covers XI1 `ListInputDevices`,
`SelectExtensionEvent`, device properties, button mapping, pointer
feedback and `SetDeviceMode`, plus XI2 grabs, `XIWarpPointer` and
`XISetFocus`/`XIGetFocus`, including under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/xi-x11-probe/install-and-run.sh
```
