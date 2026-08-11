# XFixes selection owner notifications

## Trigger and protocol boundary

WPS cold startup issued XFixes minor opcode 2 three times. This is
`SelectSelectionInput`, used by Qt to observe clipboard/selection ownership.
Returning success without publishing events would make clipboard state stale,
so the implementation is tied to a real owner-change producer.

The X.Org XFixes event is exactly 32 bytes and carries the extension event
code, subtype, subscriber window, owner, selection atom, event time, and
selection-change time. BionicX stores subscriptions per client/window/atom,
replaces or clears them on repeated requests, and frees them with the client.

Chrome subsequently exposed that complete XFixes 2.0 clients subscribe with
mask 7. The selection manager now retains the owning client and acquisition
timestamp, distinguishes explicit owner-window destruction from connection
shutdown, and emits `SetSelectionOwnerNotify`,
`SelectionWindowDestroyNotify`, or `SelectionClientCloseNotify` only to the
corresponding mask. A closing subscriber is not sent its own close event.
Unknown mask bits still receive `BadValue`.

## Controlled proof

The genuine AArch64 glibc desktop probe subscribes to CLIPBOARD with mask 7. It
validates an owner change, destroys that owner window and validates the destroy
reason, then makes a second X connection own the selection and validates the
client-close reason when that connection closes. It reports
`mask=7 set=1 destroy=1 close=1`; the complete desktop suite remains 8/8 with
zero X errors in `evidence/x11-desktop-probe-xfixes-mask7.log`.

## Real application result

A cold WPS startup after installation contains no unimplemented XFixes request,
Qt XKB error, process exit, or Android crash. The live home UI is retained in
`evidence/wps-xfixes-selection-notify.png`; the filtered assertion is
`evidence/wps-xfixes-selection-notify.log`.

This removes the last known XFixes startup warning. End-to-end Android/X11
clipboard transfer remains a separate workflow because it also requires core
ConvertSelection, property transfer, target negotiation, and Android clipboard
bridging.

Chrome previously produced exactly three `SelectSelectionInput(mask=7)` errors
on each cold launch. The same full-screen DNS/TLS navigation run after this
change produced zero, while Example Domain remained rendered and interactive.
See `evidence/chrome-xfixes-mask7.log` and
`evidence/chrome-xfixes-mask7.png`.
