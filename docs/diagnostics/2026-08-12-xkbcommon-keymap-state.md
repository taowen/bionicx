# XKB keymap and state through real xkbcommon

## Symptom

The libX11 XKB probes passed, but Qt in WPS first logged `failed to compile a
keymap`. Adding more map fields by retrying WPS was too indirect to identify
which semantic condition failed.

## Controlled reproducer

`x11-desktop-probe` now links the genuine AArch64 glibc
`libxkbcommon-x11.so.0` and calls both
`xkb_x11_keymap_new_from_device()` and `xkb_x11_state_new_from_device()`.
The first run converted the opaque Qt failure into three exact missing XKB
requests:

- minor 13, `GetIndicatorMap`;
- minor 10, `GetCompatMap`;
- minor 6, `GetControls`.

After those replies were implemented, xkbcommon still rejected the map. The
decisive source comparison found that the map-component bits had been assigned
using the wrong positions. libX11 accepted the reply, but xkbcommon requires
the exact set `KeyTypes | KeySyms | ModifierMap | ExplicitComponents |
KeyActions | VirtualMods | VirtualModMap` (`0x00df`).

## Fix

The X server now returns the complete required component mask, a full key
symbol/action range, key-type level names, and coherent empty compatibility and
indicator maps. `GetControls` reports one group and per-key repeat state.
`GetState` supplies the initial zero modifier/group state required when Qt
constructs its xkbcommon state.

The retained controlled result is
`evidence/x11-desktop-probe-xkbcommon-state.log`: six strict checks pass, the
compiled keymap resolves `K038` to `XK_a`, and its state resolves the same key.
On WPS startup there are zero Qt XKB errors and zero unimplemented XKB
requests; see `evidence/wps-xkbcommon-state.log`.

## WPS result and next gap

Writer now accepts Android-injected keyboard input in a focused document. The
visible result is `evidence/wps-xkb-state-input.png`.

This exposed the next, narrower input gap: letters currently arrive in upper
case and shifted punctuation is not selected (underscore arrived as minus).
`evidence/wps-xkb-state-caps2.png` records that behavior. The follow-up core
event and key-action tests are in `2026-08-12-xkb-modifier-actions.md`.
