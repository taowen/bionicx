# X11 desktop extension probe

This genuine AArch64 glibc client links the desktop X11 libraries used by
Chromium and Qt applications. It performs both version negotiation and one
stateful request sequence for Render, XFixes, RandR, XInput2 and XKB. Its XI2
sequence selects master input events on a window and reads the masks back. Its
RandR sequence cross-checks the single output, CRTC, mode, physical size,
primary output and an absent output property, and selects all 1.3 event masks.
The XKB
sequence includes selecting core-keyboard state notifications, reading its
device metadata, and then reading a coherent keymap and keyboard-name set.
It also links the real AArch64 glibc `libxkbcommon-x11`, compiles that server
map with the same API used by Qt, creates an XKB state, and resolves a key
through the compiled state. Deterministic Android input is then observed from
the XCB event queue to verify live XKB StateNotify set/clear transitions. An
XFixes selection subscription additionally validates an owner-change event,
while a ShapeInput region proves copy lifetime and pointer hit-test clipping
with one tap outside and one inside the region. This stricter path catches
semantic protocol gaps which libX11's structure-only helpers accept.
MIT-SHM
is reported as an optional capability because BionicX deliberately withholds
it until a safe shared-memory backend exists.

The server currently provides stateful Render 0.1 support. The probe's strict
summary is the capability ledger for the remaining desktop extensions. The
probe currently has eight strict checks.

```sh
examples/x11-desktop-probe/build-bundle.sh
ANDROID_SERIAL=<serial> examples/x11-desktop-probe/install-and-run.sh
adb -s <serial> logcat -d -s BionicX | grep -E 'BX(TEST|CAP|SUMMARY|XERROR)'
```
