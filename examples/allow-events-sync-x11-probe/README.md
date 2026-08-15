# AllowEvents Sync probe

libX11 only. `XAllowEvents` must accept `SyncPointer`, `SyncKeyboard`
and `SyncBoth`, including a nonzero timestamp and under `GrabServer`.
GDK thaws `GrabModeSync` this way.

This is separate from `allow-events-x11-probe`, which covers Async*
and `ReplayKeyboard`.

```sh
ANDROID_SERIAL=<serial> examples/allow-events-sync-x11-probe/install-and-run.sh
```
