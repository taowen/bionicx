# Core X11 integration probe

This is a genuine AArch64 glibc/libX11 client, not a hand-written wire-protocol
test. It exercises core requests used underneath larger Qt and Chromium
clients and emits stable `BXTEST` records to Android logcat.

Covered operations:

- display setup and extension enumeration;
- top-level and child window creation, mapping, query-tree and translation;
- atoms, WM properties and an exact UTF-8 property round trip;
- GC, Pixmap rendering and `CopyArea`;
- a custom cursor and CLIPBOARD selection ownership;
- synthetic ClientMessage delivery and observation of real key/touch events.

Run on a connected device:

```sh
examples/x11-probe/build-bundle.sh
ANDROID_SERIAL=<serial> examples/x11-probe/install-and-run.sh
adb -s <serial> logcat -d -s BionicX | grep -E 'BX(TEST|INFO|SUMMARY)'
```

Input is observational: lack of user input during the 12-second run is logged
but does not fail the automated core-protocol result.
