# XKB keyboard-by-name composite reply

Opening a Writer document caused WPS to send XKB minor opcode 23,
`GetKbdByName`. This request does not return an ordinary map reply: its outer
reply advertises components and embeds complete component replies in a fixed
order.

BionicX now reports its client/server symbols and required key types and embeds
the same printable `GetMap` model used by the direct request. The map writer is
shared so the two protocol paths cannot silently diverge.

The AArch64 glibc probe calls `XkbGetKeyboard`, which uses opcode 23, and
verifies `a/A` from the nested reply. Its 5/5 result is in
`evidence/x11-desktop-probe-xkb-get-by-name.log`.

At this checkpoint Qt still reported `failed to compile a keymap` and injected
text did not enter the focused document. The focused Writer state is captured
in `evidence/wps-xkb-get-by-name-focused.png`, with the diagnostic line in
`evidence/wps-xkb-get-by-name.log`. The follow-up investigation and resolution
are recorded in `2026-08-12-xkbcommon-keymap-state.md`.
