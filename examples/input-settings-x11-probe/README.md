# Input settings probe

libX11 only. One two-connection client covers the settings-daemon
input contract: pointer acceleration, core keyboard control, pointer
mapping, XKB repeat, lock/latch, named Caps/Num indicators, bell and
detectable auto-repeat, including under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/input-settings-x11-probe/install-and-run.sh
```
