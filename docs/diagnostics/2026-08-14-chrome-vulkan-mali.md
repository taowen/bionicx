# Chrome ANGLE Vulkan on Mali-G1-Ultra

Device `10AFA31610002QH` (V2509A / PD2509, Android 16, `vulkan.mali.so`)
keeps seed `ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.

ANGLE on X11 matches the Winlator TrueColor visual to
`VK_FORMAT_B8G8R8A8_UNORM`. Client swapchain images are ordinary
DEVICE_LOCAL images in that format. Present converts with
`vkCmdBlitImage` into a DEVICE_LOCAL RGBA image, copies that into a
HOST_VISIBLE buffer, and the GLES compositor uploads the copy. The
GL thread never locks the window AHardwareBuffer.

Acquire does not return an image that is the current blit source or
the mailbox pending image. One blit is in flight; extra presents keep
only the latest index. The blit is submitted after the client's
render `vkQueueSubmit` on the single graphics queue, so present-wait
semaphores are not GPU-waited (a later timeline wait on those
semaphores would hang the blit).

`vkWaitForFences` with `timeout != 0` never blocks the RPC thread.
An already-signaled fence is a signaled eventfd, not `fd=-1` over
SCM_RIGHTS. A not-ready fence is an unsignaled eventfd plus an
off-thread `vkWaitForFences`; `GetFenceFd` is not used for this wait
because SYNC_FD export transfers the payload and leaves the fence
unsignaled. `vkResetFences` waits for the server.

`vulkan-lifetime-deferred-fence` waits on an unsignaled fence while
another thread later `vkQueueSubmit`s it. That hangs if RPC blocks
inside `WaitForFences`. It PASSes on this device.

`vulkan-chrome-frames` (1920×1080, acquire sem+fence, 16 overlapping
presents, green then blue) is `16/16` and compositor
`blue≈1.8e6 green=0` on this device. The same probe is blue on Adreno
`HA27DTL0`.

Untraced `chrome-vulkan.json` paints the static WebGL fixture
(`WEBGL_OK` / ANGLE Vulkan Mali). `chrome-smoke` plus `open-gpu.sh`
paints the full `chrome://gpu` page (Graphics Feature Status,
ANGLE/Vortek Mali-G1-Ultra). Adreno `HA27DTL0` still paints the same
page after the WaitForFences change.

See `evidence/vivo-10AFA31610002QH/chrome-smoke.png` and
`evidence/HA27DTL0/chrome-smoke.png`.
