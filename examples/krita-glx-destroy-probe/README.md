# Krita GLX DestroyContext(NULL) probe

Untraced Krita 5.2 exits 139 in Gladio `glXDestroyContext` →
`SparseArray_free` (`libGL.so.1.7.0+0x4f510`, fault `0x220`). The
caller is Qt `libqxcb-glx-integration` from `libkritaui`. GLX treats a
NULL context as a no-op; Qt destroys unused/failed `QGLXContext`
wrappers that way.

```sh
ANDROID_SERIAL=<serial> examples/krita-glx-destroy-probe/install-and-run.sh
```

Expect `BXSUMMARY krita-glx-destroy passed=4 failed=0` (`destroy-null`,
`choose-fbconfig`, `create-new`, `destroy-created`).
