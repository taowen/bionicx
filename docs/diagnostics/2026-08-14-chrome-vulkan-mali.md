# Chrome ANGLE Vulkan on Mali-G1-Ultra

Device `10AFA31610002QH` (V2509A / PD2509, Android 16, `vulkan.mali.so`)
keeps seed `ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.

RGBA-only surface formats made `vulkan-probe` pass by falling back to
`formats[0]`, but Chrome `--use-angle=vulkan` then died with
`eglCreateWindowSurface … EGL_BAD_MATCH` (exit 139). ANGLE on X11 matches
the Winlator TrueColor visual (`red=0xff0000`) to
`VK_FORMAT_B8G8R8A8_UNORM`. Mali still cannot color-attach a BGRA
AHardwareBuffer, so the ICD advertises BGRA, allocates client images as
ordinary DEVICE_LOCAL images in the requested format, and
`vkCmdBlitImage`s into an RGBA window AHB for the GLES compositor.

`vulkan-probe` is 9/9 on this device. `host-vulkan-window` /
`host-vulkan-surface` / `host-vulkan-swapchain` require the X11 BGRA
visual, advertised `VK_FORMAT_B8G8R8A8_UNORM`, and swapchain `format=44`.
Untraced `chrome-vulkan.json` paints the fixture green and
reports `WEBGL_OK vendor=Google Inc. (ARM) renderer=ANGLE (ARM, Vulkan
1.3.128 (Vortek (Mali-G1-Ultra MC12) (0xE8800010)), Mali-G1-Ultra MC12)`.
The fixture now uses `WEBGL_debug_renderer_info` so a masked
`WebKit WebGL` string cannot pass as Vulkan.

Chrome's GPU watchdog kills the child every 30s (`exit_code=512`)
because Vortek RPC shader/pipeline work does not ping as a native
GPU would. The profile sets `--disable-gpu-watchdog` on argv and
`CHROME_EXTRA_FLAGS` so the child keeps it. Present blit then
restores the client image to `PRESENT_SRC` so ANGLE's next frame
does not hang Mali. After 52s the green canvas and `WEBGL_OK` string
remain; ownership is 0.

See `evidence/vivo-10AFA31610002QH/vulkan-probe.png` and
`evidence/vivo-10AFA31610002QH/chrome-vulkan.png`.
