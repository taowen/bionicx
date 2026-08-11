# XFixes selection owner notifications

## Trigger and protocol boundary

WPS cold startup issued XFixes minor opcode 2 three times. This is
`SelectSelectionInput`, used by Qt to observe clipboard/selection ownership.
Returning success without publishing events would make clipboard state stale,
so the implementation is tied to a real owner-change producer.

The X.Org XFixes event is exactly 32 bytes and carries the extension event
code, subtype, subscriber window, new owner, selection atom, event time, and
selection-change time. BionicX now stores subscriptions per client/window/atom,
replaces or clears them on repeated requests, frees them with the client, and
sends subtype `SetSelectionOwnerNotify` only to matching masks.

Only `SetSelectionOwnerNotifyMask` is accepted at this checkpoint. The distinct
window-destroy and client-close reasons are rejected with `BadValue` until the
selection manager preserves those lifecycle causes; they are not silently
advertised.

## Controlled proof

The genuine AArch64 glibc desktop probe subscribes its window to CLIPBOARD,
becomes the selection owner, and validates the libXfixes-decoded subtype,
subscriber window, owner window, and atom. The XFixes check reports
`selection-notify=1` and the complete desktop suite remains 8/8 in
`evidence/x11-desktop-probe-xfixes-input-shape.log`.

## WPS result

A cold WPS startup after installation contains no unimplemented XFixes request,
Qt XKB error, process exit, or Android crash. The live home UI is retained in
`evidence/wps-xfixes-selection-notify.png`; the filtered assertion is
`evidence/wps-xfixes-selection-notify.log`.

This removes the last known XFixes startup warning. End-to-end Android/X11
clipboard transfer remains a separate workflow because it also requires core
ConvertSelection, property transfer, target negotiation, and Android clipboard
bridging.
