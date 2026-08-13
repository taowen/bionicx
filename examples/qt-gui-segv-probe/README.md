# Qt GUI SIGSEGV probe (XTEST + post-makeCurrent)

KeePassXC auto-type calls `XQueryExtension("XTEST")` and
`XQueryExtension("XInputExtension")` after `dlopen` of
`libkeepassxc-autotype-xcb.so` (NEEDED `libXtst.so.6`). Krita and KeePassXC
then SIGSEGV after a successful `glXMakeCurrent`. This Gladio client checks
the XTEST/libXtst/autotype load path and draws after makeCurrent.

Expect `BXSUMMARY qt-gui-segv passed=7 failed=0`.

```sh
ANDROID_SERIAL=<serial> examples/qt-gui-segv-probe/install-and-run.sh
```
