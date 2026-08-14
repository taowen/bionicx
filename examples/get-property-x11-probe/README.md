# GetProperty / GetAtomName probe

libX11 only. `XGetWindowProperty` with `long_length=LONG_MAX` must return
an existing `_NET_WM_WINDOW_TYPE` ATOM list and a 12-cardinal
`_NET_WM_STRUT_PARTIAL` without `BadValue`. `XGetAtomName(None)` and an
unknown atom must raise `BadAtom` instead of hanging.

This is the xfwm4 `getAtomList` / `getCardinalList` contract used on
every mapped client, including TYPE_DOCK panels.

```sh
ANDROID_SERIAL=<serial> examples/get-property-x11-probe/install-and-run.sh
```
