# XKB event selection

WPS/Qt printed `failed to select notify events from XKB`, but the previous
server logs did not identify failed extension requests. Protocol-level error
logging showed the exact request:

```text
unimplemented request=XKEYBOARD major=148 minor=1 error=17
```

Minor opcode 1 is `SelectEvents`. BionicX now parses its fixed wire request,
validates the core keyboard selector, and keeps the selected event and map
masks per X client. Event emission will consume this state when the XKB event
path is added.

The genuine AArch64 glibc desktop probe now calls `XkbSelectEvents` through
libX11 and synchronizes to surface asynchronous protocol errors. On device it
passes all five desktop-extension groups with zero X errors; the raw result is
in `evidence/x11-desktop-probe-xkb-select-events.log`.

The WPS rerun no longer prints the event-selection warning. Its remaining XKB
startup failure is independently identified as minor opcode 24,
`GetDeviceInfo`; see `evidence/wps-xkb-select-events-fixed.log`.
