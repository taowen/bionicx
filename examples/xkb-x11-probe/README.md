# XKB probe

libX11 XKB plus libxkbcommon-x11. One client reads the core keyboard map
and device name, compiles that map with the same xkbcommon API Qt uses,
then `XTestFakeKeyEvent`s Shift and requires live StateNotify set/clear.
Does not start a desktop daemon and does not wait for Android taps.

```sh
ANDROID_SERIAL=<serial> examples/xkb-x11-probe/install-and-run.sh
```
