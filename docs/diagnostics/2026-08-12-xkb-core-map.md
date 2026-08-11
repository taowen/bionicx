# Minimal read-only XKB core map

## Controlled transaction

The glibc/libX11 desktop probe negotiates XKB 1.0 and calls
`XkbGetMap(XkbAllClientInfoMask, XkbUseCoreKbd)`. Success requires libX11 to
allocate and safely free the variable-length client map, not merely accept the
extension version. The test validates the legal keycode range, required key
types, symbol-map offset and the Escape mapping (`XK_Escape`, `0xff1b`).

## Diagnostic trap: required key types

The first stateful reply contained one valid-looking one-level key type. libX11
returned status 11 (`BadAlloc`) even though memory was available. Its
`XkbAllocClientMap` rejects any nonzero type count smaller than
`XkbNumRequiredTypes` (4); `_XkbReadGetMapReply` then translates every allocation
helper failure to `BadAlloc`. Returning four required types resolved the false
memory symptom.

This distinction was exposed by temporarily using `XkbGetUpdatedMap` to retain
the exact status. The final regression uses the normal `XkbGetMap` API again.

## Implemented boundary

`XKeyboardExtension` now supplies:

- `UseExtension` for XKB 1.0;
- `GetMap` with keycodes 8 through 255;
- four legal one-level required key types;
- one symbol record mapping keycode 9 to `XK_Escape`.

This is a structurally valid seed map, not yet a complete keyboard layout.
Full core keysyms, modifier maps, state/control requests, names and XKB event
delivery remain later integration targets.

## x300 proof

```text
BXTEST PASS xkeyboard version=1.0 opcode=148 status=0 keys=8-255 types=4 syms=1 offset=1 escape=0xff1b
BXSUMMARY desktop-x11 passed=5 failed=0 xerrors=0
```

The ordinary untraced run is retained in
`evidence/x11-desktop-probe-xkeyboard.log`.
