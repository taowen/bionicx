# Cross-client X11 keyboard-grab probe

This genuine AArch64 glibc/libX11 client opens independent grabber and peer
connections. It checks `GrabKeyboard` success, cross-client contention,
`GrabNotViewable`, rerouting of an Android-injected A key away from the focused
peer, restoration of normal B-key delivery after `UngrabKeyboard`, and cleanup
when a grabbing client disconnects. A third injected key verifies that
`owner_events=True` preserves normal delivery to another window owned by the
grabbing client.

```sh
ANDROID_SERIAL=<serial> examples/keyboard-grab-x11-probe/install-and-run.sh
```

Set `BIONICX_SCREENSHOT=/path/to/output.png` to capture the labeled result
windows after the summary and before the client exits.

Success requires seven strict checks, zero X errors, and a normal process exit.
The two injected keys are physical server input events, not `XSendEvent`
shortcuts.
