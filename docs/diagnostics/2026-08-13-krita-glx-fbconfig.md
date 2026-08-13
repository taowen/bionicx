# Krita Qt GLX FBConfig

## Symptom

Untraced Krita 5.2 on the shared seed exited 139. Logcat showed Qt
`qglx_findConfig` failing for `QSurfaceFormat` 3.3 Compatibility with
`swapBehavior=SingleBuffer`.

## Controlled client

`examples/qt-glx-fbconfig-probe` is a Gladio `libGL` client (the same host
GL path as `glx-probe`, not Debian mesa). After the X server advertises a
SingleBuffer FBConfig next to the original double-buffered one:

```text
BXSUMMARY qt-glx-fbconfig passed=3 failed=0
```

`configs=2 single=1 double=1` and `glXCreateContextAttribsARB` 3.3 succeeds.

The activity now injects `BIONICX_APP` and, when `apps/<id>/lib` exists,
`LD_LIBRARY_PATH` so Krita's `DT_NEEDED libGL.so.1` can resolve Gladio from
the profile payload. Profiles still cannot set `LD_LIBRARY_PATH` themselves.

## Still failing

Krita still exits 139 with the same `qglx_findConfig` SingleBuffer miss.
The probe's 3.3 context is not the same as Qt's full format match (depth,
alpha, stencil, samples, profile). That remaining Qt filter is not claimed.
