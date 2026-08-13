# Qt / Krita GLX FBConfig probe

Krita's Qt xcb platform calls `glXChooseFBConfig` with `GLX_DOUBLEBUFFER=False`
(SingleBuffer) before it will create a 3.3 compatibility context. This probe is
a Debian mesa `libGL` client on the shared seed, not Gladio.

```sh
ANDROID_SERIAL=<serial> examples/qt-glx-fbconfig-probe/install-and-run.sh
```
