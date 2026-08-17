# Core X11 integration probe

This is a genuine AArch64 glibc/libX11 client, not a hand-written wire-protocol
test. It exercises core requests used underneath larger Qt and Chromium
clients and emits stable `BXTEST` records to Android logcat.

Covered operations:

- display setup and extension enumeration;
- top-level and child window creation, mapping, query-tree and translation;
- atom creation plus the `only_if_exists`/`None` contract, WM properties and
  an exact UTF-8 property round trip plus property-name enumeration;
- GC, Pixmap rendering, `CopyArea`, both PolyPoint coordinate modes, tiled
  window-background pixmaps used by `ClearArea`, and the xfwm4 decoration
  path of `SetBG` then `Map` plus `MoveResize` without `ClearWindow`;
- a custom cursor and CLIPBOARD selection ownership;
- cursor-font open/close resource lifetime plus fixed-font query, text metrics
  and font-name enumeration;
- root TrueColor allocation/query round trips and cursor recoloring;
- initial/dynamic `PointerRoot` focus, independent focus revert policy and
  exact lower/upper keysym pairs from the core keyboard map;
- synthetic ClientMessage delivery and real Android key/touch events;
- strict core modifier-state semantics for lower-case, Shift press/release and
  shifted underscore input.

Run on a connected device:

```sh
examples/x11-probe/build-bundle.sh
ANDROID_SERIAL=<serial> examples/x11-probe/install-and-run.sh
adb -s <serial> logcat -d -s BionicX | grep -E 'BX(TEST|INFO|SUMMARY)'
```

`install-and-run.sh` injects `abc_A`, a tap and a swipe. The modifier sequence
is a strict assertion; pointer counts remain observational.
