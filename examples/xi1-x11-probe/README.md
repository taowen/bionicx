# XI1 ListInputDevices probe

libXi only, no settings daemon. `XListInputDevices` must return a core
pointer and keyboard. `XSelectExtensionEvent` with an empty class list
must succeed, including under `GrabServer`.

This is the xfsettingsd startup contract.

```sh
ANDROID_SERIAL=<serial> examples/xi1-x11-probe/install-and-run.sh
```
