# X11 desktop extension probe

This genuine AArch64 glibc client links the desktop X11 libraries used by
Chromium and Qt applications. It performs both version negotiation and one
stateful request sequence for Render, XFixes, RandR, XInput2 and XKB. The XKB
sequence includes selecting core-keyboard state notifications before reading
the keymap. MIT-SHM
is reported as an optional capability because BionicX deliberately withholds
it until a safe shared-memory backend exists.

The server currently provides stateful Render 0.1 support. The probe's strict
summary is the capability ledger for the remaining desktop extensions.

```sh
examples/x11-desktop-probe/build-bundle.sh
ANDROID_SERIAL=<serial> examples/x11-desktop-probe/install-and-run.sh
adb -s <serial> logcat -d -s BionicX | grep -E 'BX(TEST|CAP|SUMMARY|XERROR)'
```
