# Live XKB StateNotify synchronization

## Remaining boundary

WPS still ignored Shift after both the core event masks and the static
xkbcommon actions were correct. Qt had selected XKB StateNotify, but BionicX
only stored that selection; it never delivered state changes, and `GetState`
always returned a zero snapshot.

## Implementation

BionicX now keeps a synchronized client registry and sends the 32-byte XKB
StateNotify wire event only to clients selecting `XkbStateNotifyMask`. The
keyboard exposes effective, depressed and locked modifier masks. `GetState`
uses those same values, so request/reply and asynchronous state cannot diverge.
Modifier events preserve X11's pre-event core state ordering, then publish the
new XKB state after the transition.

The desktop probe injects `abc_A`, takes ownership of libX11's underlying XCB
event queue after setup, and checks the raw extension events. This avoids an
important diagnostic trap: libX11 may filter extension events according to its
own selected-detail bookkeeping, whereas Qt consumes XCB events. The retained
`evidence/x11-desktop-probe-xkb-state-notify.log` proves Shift set and clear,
with correct keycode, core event type, base/effective/locked masks and changed
components. All seven desktop checks pass.

## WPS result

Writer now displays the exact injected string `BionicX_WPS_2026`, including
mixed case and shifted underscore. The visual result is
`evidence/wps-xkb-state-notify.png`; startup has no Qt XKB error or unimplemented
XKB request. Keyboard construction, initial state, actions and live modifier
state are therefore no longer blockers for the first WPS workflow.
