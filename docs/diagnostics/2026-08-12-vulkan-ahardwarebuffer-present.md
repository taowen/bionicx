# glibc Vulkan command submission and AHardwareBuffer presentation

The controlled Vortek client now exercises the complete first-frame data path,
not only Vulkan discovery. It selects the real graphics queue, creates a
logical device with `VK_KHR_swapchain`, creates a one-image 640x360 Xlib
swapchain, and obtains the server-backed swapchain image. The Bionic server
creates that image from the X window's app-private `AHardwareBuffer` and binds
it to memory imported by Android's vendor Vulkan driver.

The glibc client records two image barriers and a
`vkCmdClearColorImage(0.10, 0.75, 0.25, 1.0)`, submits it on the host graphics
queue, waits for completion, and presents the image. The runner collapses the
Android status shade and reads the final `screencap` PNG. It requires more than
200,000 target-color pixels spanning the expected 640x360 X window, so Vulkan
success codes without a real compositor update cannot pass.

## Startup timing found by the test

An early single present can occur before the Activity's GLES compositor has
finished creating its Android surface. Vortek currently treats present as a
window-content invalidation, so that first invalidation may precede the first
renderer frame. The probe keeps presenting the completed image for five
seconds, which models a real application's frame loop and makes the end-to-end
assertion deterministic. A later lifecycle test should cover pause/resume and
surface recreation explicitly.

## Controlled verification

On x300 `01408BH601027129`, untraced and under the ordinary application UID:

```text
BXTEST PASS host-vulkan-logical-device status=0 handle=valid family=0
BXTEST PASS host-vulkan-device-queue handle=valid family=0 index=0
BXTEST PASS host-vulkan-swapchain status=0 handle=valid format=44 extent=640x360 images=1
BXTEST PASS host-vulkan-swapchain-images status=0 advertised=1 returned=1
BXTEST PASS host-vulkan-command-buffer pool=0 allocate=0 handle=valid
BXTEST PASS host-vulkan-acquire status=0 index=0 count=1
BXTEST PASS host-vulkan-record-clear status=0 color=26,191,64,255
BXTEST PASS host-vulkan-submit-clear submit=0 idle=0
BXTEST PASS host-vulkan-present status=0 index=0 color=26,191,64,255
BXSUMMARY host-vulkan passed=23 failed=0
BXTEST PASS host-vulkan-compositor pixels=230293 bounds=0,0-639,359 size=1920x1080
```

Android logs identify `/vendor/lib64/hw/vulkan.adreno.so`; the screenshot
contains the exact `(26,191,64)` clear color. This proves that command bytes
originating in a glibc ELF reached the proprietary Bionic driver and that its
rendered AHardwareBuffer reached BionicX's X11 and Android compositors.
