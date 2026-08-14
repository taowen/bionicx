# AllowEvents probe

libX11 only. `XAllowEvents` must accept `AsyncPointer`, `AsyncKeyboard`,
`AsyncBoth` and `ReplayKeyboard`, including a nonzero timestamp and while
the client holds `GrabServer`. GTK uses this after `GrabKey` Sync.

```sh
ANDROID_SERIAL=<serial> examples/allow-events-x11-probe/install-and-run.sh
```
