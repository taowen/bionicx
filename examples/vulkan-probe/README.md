# Host Vulkan probe

This genuine AArch64 glibc client loads the pinned BionicX Vortek ICD through
the ordinary Linux Vulkan loader. Vortek transports Vulkan calls over an
app-private Unix socket to the Bionic Android host, which loads the system
Vulkan loader and the device's native vendor driver.

The probe verifies loader and instance discovery, physical-device identity,
queue families, device memory, and both Xlib and XCB WSI paths. It creates a
real X window through Xlib, binds both surface APIs to its XID, then creates a
logical device and swapchain, imports the window's
`AHardwareBuffer`, uploads interleaved positions/colors through mapped
host-visible Vulkan memory, builds real SPIR-V vertex/fragment shaders and a
graphics pipeline, records a render pass with a red triangle over a green background,
presents it through a semaphore without a prior queue idle, then checks
the final pixels in an Android compositor screenshot.

```sh
ANDROID_SERIAL=<serial> examples/vulkan-probe/install-and-run.sh
```

Set `BIONICX_SCREENSHOT=path.png` to retain the screenshot.
