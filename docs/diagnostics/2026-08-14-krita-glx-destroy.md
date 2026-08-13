# Krita GLX DestroyContext(NULL)

Untraced Krita 5.2 (no args and with `bionicx-image.ppm`) exited 139:

```text
signal=11 address=0x220
pc  libGL.so.1.7.0 SparseArray_free +0x10
lr  libGL.so.1.7.0 glXDestroyContext
    libqxcb-glx-integration.so
    libQt5Gui.so.5.15.15
    libkritaui.so.19
```

`glXDestroyContext` did `SparseArray_free(&ctx->clientState.vertexArrays)`
at `ctx+0x218` with `ctx == NULL` (fault `0x220`). GLX 1.3 treats a NULL
context as a no-op; Qt xcb-glx destroys unused/failed `QGLXContext`
wrappers that way.

## Controlled client

`examples/krita-glx-destroy-probe` calls `glXDestroyContext(dpy, NULL)`,
then `glXCreateNewContext` / destroy. Device:

```text
BXSUMMARY krita-glx-destroy passed=4 failed=0
```

Gladio now returns immediately on a NULL ctx and accepts a NULL
`attrib_list` in `glXCreateContextAttribsARB` (`glXCreateNewContext`).

Untraced Krita then stays up and shows `bionicx-image.ppm` (640×480).
