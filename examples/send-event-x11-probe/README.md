# SendEvent PointerWindow / InputFocus probe

libX11 only. One two-connection client covers `XSendEvent` to
`PointerWindow` (0) and `InputFocus` (1): origin delivery, `None`
discard, `PointerRoot` focus, pointer inside the focus window,
event-mask routing, propagate to an ancestor, and `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/send-event-x11-probe/install-and-run.sh
```
