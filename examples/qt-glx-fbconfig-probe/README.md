# Qt / Krita GLX FBConfig probe

Krita's Qt xcb platform calls `glXChooseFBConfig` with the `qglx_buildSpec`
list (LEVEL 0, RGB ≥ 1, ALPHA/DEPTH/STENCIL 0, WINDOW_BIT, SingleBuffer by
omitting `GLX_DOUBLEBUFFER`) before it will create a 3.3 compatibility
context. This probe is a Gladio `libGL` client on the shared seed, the same
host GL path as Krita, not Debian mesa.

```sh
ANDROID_SERIAL=<serial> examples/qt-glx-fbconfig-probe/install-and-run.sh
```
