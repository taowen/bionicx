# Core X11 passive pointer grabs

Unmodified Debian ARM64 IceWM issued 46 core opcode 28 requests and four opcode
29 requests while installing its mouse bindings. Treating them as no-ops would
allow startup but leave clicks routed to the wrong client.

BionicX now retains asynchronous `GrabButton` registrations by owner and
window, supports exact and wildcard button/modifier matching, walks the pointer
window's ancestors, and activates the matching grab until all buttons are
released. Cross-client conflicts return `BadAccess`; disconnect and window
destruction remove both active and passive state. `owner_events=True` preserves
normal routing when the selected target belongs to the grabbing client.

The implementation also corrected inherited pointer-event routing: active
grabs now send only through the grab listener, ButtonRelease uses its actual
event-selection bit, and the event `state` field contains modifier/button state
rather than pointer-motion selection bits.

Cursor-font `OpenFont`/`CreateGlyphCursor` resources can now be attached to an
active or passive pointer grab, and the renderer uses the grab cursor while the
grab is active. The current cursor-font raster is an intentionally visible
crosshair fallback; exact standard cursor glyph shapes remain future work.

The controlled AArch64 glibc/libX11 probe opens independent grabber and peer
connections and drives three physical Android taps:

```text
BXTEST PASS passive-button-grab route=1
BXTEST PASS glyph-cursor-grab resource=4194307
BXTEST PASS passive-button-contention bad-access=1
BXTEST PASS passive-button-replay click-through=1
BXTEST PASS passive-button-owner-events normal-route=1
BXTEST PASS passive-button-ungrab normal-route=1
BXSUMMARY pointer-grab-x11 passed=6/6 xerrors=0
```

Parameter diagnostics showed IceWM's four remaining opcode-28 errors were
`GrabModeSync` pointer registrations (`pointer=0 keyboard=1 confineTo=0
cursor=0`), not cursor overrides. BionicX now freezes subsequent core release
and motion events for those grabs and implements opcode 35 `ReplayPointer`:
the initiating press is delivered first to the manager, then replayed to the
normal client without re-triggering the passive grab, followed by queued events.
The probe deliberately waits 250 ms before `XAllowEvents`, ensuring the physical
release is frozen and drained only after the replayed press.

Core X11 remains 22/22, keyboard grabs remain 9/9, and a fresh ordinary-app-UID
IceWM run remains 3/3 with no unsupported opcodes or request errors. Other
`AllowEvents` modes and non-None confinement remain explicit errors.
