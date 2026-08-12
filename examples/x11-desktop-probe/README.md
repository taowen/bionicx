# X11 desktop extension probe

This genuine AArch64 glibc client links the desktop X11 libraries used by
Chromium and Qt applications. It performs both version negotiation and one
stateful request sequence for Render, XFixes, RandR, XInput2 and XKB. Its XI2
sequence verifies the master pointer's ButtonClass, calls XIQueryPointer,
selects master input events on two top-level windows, reads the masks back,
actively grabs both master devices, and decodes real motion, button and
Enter/Leave GenericEvents injected from Android. It verifies the XI2 wire
lengths, owner-events routing and pre-transition button-state masks. Its
RandR sequence cross-checks the single output, CRTC, mode, physical size,
primary output and an absent output property, and selects all 1.3 event masks.
The XKB
sequence includes selecting core-keyboard state notifications, reading its
device metadata, and then reading a coherent keymap and keyboard-name set.
It also links the real AArch64 glibc `libxkbcommon-x11`, compiles that server
map with the same API used by Qt, creates an XKB state, and resolves a key
through the compiled state. Deterministic Android input is then observed from
the XCB event queue to verify live XKB StateNotify set/clear transitions. An
XFixes selection subscription additionally validates the complete mask 7
owner-change, owner-window-destroy and owner-client-close lifecycle, while a
ShapeInput region proves copy lifetime and pointer hit-test clipping with one
tap outside and one inside the region. This stricter path catches
semantic protocol gaps which libX11's structure-only helpers accept.
MIT-SHM
is reported as an optional capability because BionicX deliberately withholds
it until a safe shared-memory backend exists.

The server currently provides stateful Render support for ARGB32 and A8
pictures, rectangle and 1-bit pixmap picture clips with clip origins, filters,
solid fills, linear gradients, and the
Clear, Src, Over, In, OutReverse, Add and Saturate Porter-Duff operations exercised by
Cairo. The Render check reads pixels back and requires exact A8 intermediate
results (`In + Add = 0x80`, `OutReverse = 0x60`, and `Saturate = 0xff`); merely accepting the
requests does not pass. The probe's strict summary is the capability ledger
for the remaining desktop extensions. It currently has nine strict checks.

```sh
examples/x11-desktop-probe/build-bundle.sh
ANDROID_SERIAL=<serial> examples/x11-desktop-probe/install-and-run.sh
adb -s <serial> logcat -d -s BionicX | grep -E 'BX(TEST|CAP|SUMMARY|XERROR)'
```
