# XKB GetMap component-mask compatibility

## Symptom

Chrome 151 requested only `KeyTypes | KeySyms` (`full=0x0003`) from XKB
`GetMap`. BionicX replied with `present=0x00df` and serialized every supported
component. Chromium's generated X11 reply parser treated the reply as malformed
and deliberately trapped before creating a browser window.

## Cause and correction

XKB `GetMap` is a partial query. The reply `present` mask, per-component counts,
reply length, and variable data must describe the components requested through
the `full` and `partial` masks. BionicX previously used its entire supported-map
mask for every request.

The server now parses the requested device and component masks, rejects unknown
devices, intersects the request with its supported mask, and emits only the
requested type, symbol, and action sections. `GetKeyboardByName` continues to
embed the complete supported map explicitly.

## Device regression

On x300 `01408BH601027129` (Android 14/API 34), the genuine glibc/Xlib client
completed all eight desktop checks after the change, including real xkbcommon
keymap/state creation, live Shift state notifications, and injected `abc_A`
input. The retained result is
`evidence/x11-desktop-probe-xkb-request-mask.log`.

The first incremental reinstall attempt also exposed an unrelated packaging
constraint: blindly retransmitting a shared rootfs can exhaust the app data
quota. The successful run reused the existing shared runtime and installed only
the 56 KiB controlled app bundle. Installation needs content-addressed reuse as
the Chrome and WPS fixtures grow.
