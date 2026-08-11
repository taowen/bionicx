# XKB printable key map

The initial XKB map contained only Escape, which was enough to validate the
wire reader but could never describe a usable desktop keyboard. WPS accepted
focus and pointer input, then ignored injected text while Qt reported
`failed to compile a keymap`.

BionicX now exposes the existing server keyboard table through XKB for
keycodes 8 through 126. Printable keys have lower/Shift symbols, while
navigation, modifier, keypad, and function keys retain their X keysyms. The
four required key types are structurally valid: `ONE_LEVEL` plus three
Shift-based two-level types. The backing Java keysym array is also correctly
sized for two symbols per keycode.

The AArch64 glibc probe verifies Escape plus keycode 38 as `a/A`; its raw 5/5
result is in `evidence/x11-desktop-probe-xkb-ascii-map.log`.

WPS still reports the xkbcommon compile failure in
`evidence/wps-xkb-ascii-map.log`. This narrows the remaining compiler input to
the missing compatibility/modifier semantics and `GetKbdByName` composite
request rather than absent printable symbols.
