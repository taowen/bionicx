# Core modifier events and XKB actions

## Two boundaries, two controlled assertions

The first WPS input run produced upper-case letters and a minus for an injected
underscore. The core glibc/libX11 probe was upgraded from observational input
counting to deterministic `abc_A` injection and strict event assertions.

It found that BionicX emitted modifier state after applying Shift on KeyPress
and after clearing it on KeyRelease. X11 defines the state field immediately
before each event. Keyboard delivery now follows that ordering. The retained
14/14 run in `evidence/x11-probe-modifier-state.log` proves:

- lower-case `a` arrives with state zero;
- Shift KeyPress has state zero;
- underscore's physical minus key carries `ShiftMask`;
- Shift KeyRelease still carries `ShiftMask`.

The next controlled check used the compiled xkbcommon state rather than core
event lookup. It exposed that BionicX advertised `KeyActions` but returned no
actions, so `xkb_state_update_key(Shift, DOWN)` could not select the second
level. The XKB map now supplies SetMods/LockMods actions for Shift, Control,
Alt, CapsLock and NumLock, and uses a real Lock-aware `ALPHABETIC` type.
`evidence/x11-desktop-probe-xkb-actions.log` records the 6/6 result, including
`minus=0x2d/0x5f` before and after simulated Shift.

## Remaining WPS distinction

Both controlled layers are correct, but WPS still renders the injected string
as `BIONICX-WPS-2026`; see `evidence/wps-xkb-modifier-actions.png`. This rules
out core state ordering and static xkbcommon actions. The remaining boundary is
live XKB state synchronization: BionicX tracks selections but does not yet emit
XKB StateNotify events, while `GetState` currently reports only a zero snapshot.
That is the next implementation target.
