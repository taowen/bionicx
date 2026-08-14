# Host Vulkan probe

This genuine AArch64 glibc client loads the pinned BionicX Vortek ICD through
the ordinary Linux Vulkan loader. The ICD JSON names the driver relative to
the manifest (`../../../lib/libvulkan_vortek.so`), and the probe also
`dlopen`s the ICD SONAME through the interposed runtime. Vortek transports Vulkan calls over an
app-private Unix socket to the Bionic Android host, which loads the system
Vulkan loader and the device's native vendor driver.

The probe reports nine stages: loader/ICD/WSI extensions, the X11 BGRA
visual, physical-device honesty, Xlib+XCB surfaces, surface BGRA/FIFO,
BGRA swapchain, graphics pipeline, present, and swapchain lifetime.
It creates a
real X window through Xlib, binds both surface APIs to its XID, then creates a
logical device and swapchain, imports the window's
`AHardwareBuffer`, uploads interleaved vertices, indices and a fragment tint
through mapped host-visible Vulkan memory, binds the uniform through a real
descriptor set, stages and samples a Vulkan texture through a combined image
sampler, builds SPIR-V shaders and a graphics pipeline, then records an indexed
render pass with a red triangle over a green background. Vertex binding uses
Vulkan 1.3 `vkCmdBindVertexBuffers2` with its optional size and stride arrays
set to `NULL`, so the RPC bridge must preserve pointer presence exactly. It
presents it through a semaphore without a prior queue idle, then submits a
timeline wait, presents again, and signals the timeline (Chrome's present
handshake; the RPC thread must not `vkQueueWaitIdle`), then checks
the final pixels in an Android compositor screenshot. It then acquires a
second swapchain image (must not reuse the presented index), resizes the
X window and expects `VK_ERROR_OUT_OF_DATE_KHR`, recreates with
`oldSwapchain`, and remaps after an unmap/foreground cycle.

The payload is installed against the existing seed rootfs. It does not
replace `/files/rootfs`.

```sh
ANDROID_SERIAL=<serial> examples/vulkan-probe/install-and-run.sh
```

Set `BIONICX_SCREENSHOT=path.png` to retain the screenshot.
