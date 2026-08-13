# Krita Qt GLX FBConfig

## Symptom

Untraced Krita 5.2 on the shared seed exited 139. Logcat showed Qt
`qglx_findConfig` failing for `QSurfaceFormat` 3.3 Compatibility with
`swapBehavior=SingleBuffer`.

## Controlled client

`examples/qt-glx-fbconfig-probe` is a Gladio `libGL` client (the same host
GL path as `glx-probe`, not Debian mesa). It now sends the exact
`qglx_buildSpec` list Krita uses for its first `QSurfaceFormat` (`GLX_LEVEL 0`,
RGB ≥ 1, ALPHA/DEPTH/STENCIL 0, `WINDOW_BIT`, SingleBuffer by omitting
`GLX_DOUBLEBUFFER`), then `glXGetVisualFromFBConfig`, a 3.3 compatibility
context, and `glXMakeCurrent` on a 64×64 window.

The X server advertises `GLX_LEVEL 0`, stereo/aux/accum 0 on every FBConfig.
Gladio treats a missing `glXGetFBConfigAttrib` property as value 0 / success
so Qt's `GLX_LEVEL 0` match no longer becomes `BAD_ATTRIBUTE`.

```text
BXSUMMARY qt-glx-fbconfig passed=5 failed=0
```

The activity injects `BIONICX_APP` and, when `apps/<id>/lib` exists,
`LD_LIBRARY_PATH` so Krita's `DT_NEEDED libGL.so.1` can resolve Gladio from
the profile payload. Profiles still cannot set `LD_LIBRARY_PATH` themselves.

## Still failing

Krita no longer logs `qglx_findConfig` (choose likely succeeds) and still
exits 139 (`status=0xb`) about 200 ms later. `libKF5Crash.so.5` is NEEDED.
The remaining crash is after makeCurrent, not another missing FBConfig field.
