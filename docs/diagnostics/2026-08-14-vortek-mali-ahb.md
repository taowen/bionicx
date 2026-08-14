# Vortek AHB present on Mali-G1

Device `10AFA31610002QH` (V2509A, Mali-G1-Ultra, `vulkan.mali.so`).
`vulkan-probe` was 40/40 including `host-vulkan-present`, but the Android
screencap stayed black (`pixels=0`). Chrome ANGLE Vulkan showed the same
black frame.

A standalone NDK client that imported an RGBA `AHardwareBuffer`, cleared it
to `(26,191,64,255)`, waited idle and locked the buffer read those exact
bytes. The write path was fine. The GLES compositor was not.

Mali does not make `PRESENT_SRC` AHB contents visible to CPU or GLES without
an explicit host-read barrier. `COLOR_OUTPUT`-only buffers also cannot be
sampled. BGRA AHB reports `vkFormat=R8G8B8A8` and is not advertised as a
swapchain format. `glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES)`
returns `GL_INVALID_OPERATION`.

The host now:

- allocates the window AHB with `GPU_COLOR_OUTPUT | GPU_SAMPLED_IMAGE |
  CPU_READ | CPU_WRITE`
- advertises only `R8G8B8A8_{UNORM,SRGB}`
- on present, barriers `PRESENT_SRC → GENERAL` with `HOST_READ` and waits
- uploads the locked AHB into a `GL_TEXTURE_2D` for the compositor

After the fix, `assert-screenshot.py` on the live present-hold frame:

```
BXTEST PASS host-vulkan-compositor pixels=174644 bounds=80,240-719,599
triangle=55756 triangleBounds=170,291-629,531 size=2640x1216
```

Evidence: `evidence/vivo-10AFA31610002QH/vulkan-probe.png`.
