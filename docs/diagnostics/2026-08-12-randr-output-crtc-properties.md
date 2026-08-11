# Complete fixed-display RandR topology reads

## Trigger and opcode correction

WPS emitted RandR minor opcodes 4, 9, 20 and 15. Reading the current protocol
header is essential because RandR retained historical opcode holes: these are
`SelectInput`, `GetOutputInfo`, `GetCrtcInfo`, and `GetOutputProperty`, not the
operations suggested by treating the table as contiguous.

Together they ask whether the resource list represents a coherent usable
display. BionicX now returns a connected output attached to its sole CRTC and
mode, physical millimeter dimensions derived from the profile DPI, rotation 0,
the same current/possible output membership, and a protocol-correct empty reply
for unpublished output properties. Invalid XIDs use RandR's BadOutput and
BadCrtc extension errors. RandR 1.3's four event-selection bits are stored per
client/window and removed with that window or client.

## Controlled proof

The genuine AArch64 glibc/libXrandr probe selects all 1.3 event masks and
cross-checks every ID returned by `XRRGetScreenResourcesCurrent` against
`XRRGetOutputInfo`, `XRRGetCrtcInfo`, and `XRRGetOutputPrimary`. Width and
height must match both the mode and CRTC; the output must be connected with one
preferred mode; and a deliberately absent property must decode as type None,
format 0, zero items and zero bytes remaining.

`evidence/x11-desktop-probe-randr-complete.log` records all eight desktop checks
passing with `output=1 crtc=1 empty-property=1` and no X error.

## WPS result and remaining boundary

Cold WPS startup reaches its home UI with zero unimplemented RandR or XFixes
requests, zero Qt XKB error, and no process/Android crash. The retained state is
`evidence/wps-randr-topology-complete.png`, with the filtered assertion in
`evidence/wps-randr-topology-complete.log`.

This completes static topology discovery; it does not claim mutable modes,
gamma, monitor/provider objects, or delivery of a topology-change event because
the BionicX Android display is still fixed during an X server instance.
