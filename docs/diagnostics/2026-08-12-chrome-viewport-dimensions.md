# Return both viewport dimensions to Chrome ANGLE

After the modern GLSL fixes, Chrome's GPU and renderer processes remained
alive and continuously submitted successful draws and swaps, but both the
Android and DevTools screenshots were black. Temporary renderer diagnostics
narrowed the failure to the host viewport: Chrome's 1919x1079 drawable was
being drawn with an actual GLES viewport of `0,0,1919,0`.

The parameter bridge allocated and returned one `GLint` for an unknown
`glGetIntegerv` query. `GL_MAX_VIEWPORT_DIMS` is a two-value query, so the host
driver wrote eight bytes into a four-byte buffer and the reply omitted the
height. ANGLE accepted the returned maximum height of zero and silently
clamped its viewport. The renderer now classifies this query as two integers.
All temporary per-draw and per-swap logging was removed after diagnosis.

## Controlled verification

The genuine glibc GLX probe now rejects a missing or implausibly small second
dimension. On x300 `01408BH601027129` it reports:

```text
BXTEST PASS host-gl-max-viewport-dimensions maximum=16384x16384
BXSUMMARY host-glx passed=26 failed=0
BXTEST PASS host-gl-compositor blue=(13, 38, 191) red=(242, 26, 13) outside=(0, 0, 0) size=1920x1080
```

Chrome was then installed from the normal `chrome-smoke` profile and launched
without signal diagnostics or Frida. The launch retained `--no-sandbox`, used
`--use-gl=angle --use-angle=gl`, and kept Skia Graphite disabled while the
native Vulkan bridge remains pending. The GPU process stayed alive and the
Android screenshot showed the complete maximized Chrome UI rather than a
black X11 surface.

The next qualification boundary is sustained user interaction and navigation
on this host-GPU path, followed by the native Vulkan bridge.
