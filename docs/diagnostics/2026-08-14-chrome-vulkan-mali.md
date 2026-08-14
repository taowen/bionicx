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

`vulkan-wsi` / `vulkan-present` / `vulkan-lifetime` are 5+3+1 on this
device. `vulkan-wsi-window` / `vulkan-wsi-surface` /
`vulkan-present-swapchain` require the X11 BGRA
visual, advertised `VK_FORMAT_B8G8R8A8_UNORM`, and swapchain `format=44`.
Untraced `chrome-vulkan.json` paints the fixture green and
reports `WEBGL_OK vendor=Google Inc. (ARM) renderer=ANGLE (ARM, Vulkan
1.3.128 (Vortek (Mali-G1-Ultra MC12) (0xE8800010)), Mali-G1-Ultra MC12)`.
The fixture now uses `WEBGL_debug_renderer_info` so a masked
`WebKit WebGL` string cannot pass as Vulkan.

`host-vulkan-present` now submits a timeline wait, presents, then
`vkSignalSemaphore`. The old present path called `vkQueueWaitIdle` on
the RPC thread, so the signal never ran and the probe hung. Present
now only GPU-waits the blit on the present semaphores. A later hang
showed up as `vulkan-frames`: after the timeline wait, 2048 presents
each submitted a blit on the same queue. Mali's `vkQueueSubmit` blocked
the RPC thread once the queue filled, so `vkSignalSemaphore` never
ran and the compositor stayed on the first green frame. Present now
mailboxes: one blit in flight, extra presents skip, and a waiter
blits the latest skipped image once the fence signals. The blit does
not wait on the client's present semaphores (Chrome parks timeline
waits there). Chrome's 30s
`exit_code=512` was the WaitIdle deadlock plus the GPU watchdog. The
profile still sets `--disable-gpu-watchdog` for slow Vortek shader
compiles. After 52s the green canvas and `WEBGL_OK` string remain;
ownership is 0.

See `evidence/vivo-10AFA31610002QH/vulkan-probe.png` and
`evidence/vivo-10AFA31610002QH/chrome-vulkan.png`. Adreno `HA27DTL0`
paints `chrome://gpu`; see
`docs/diagnostics/2026-08-14-chrome-adreno.md`.
