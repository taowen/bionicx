# Host Vulkan probe

This genuine AArch64 glibc client loads the pinned BionicX Vortek ICD through
the ordinary Linux Vulkan loader. Vortek transports Vulkan calls over an
app-private Unix socket to the Bionic Android host, which loads the system
Vulkan loader and the device's native vendor driver.

The probe verifies loader and instance discovery, physical-device identity,
queue families, device memory, and the complete Xlib WSI path. It creates a
real X window, logical device and swapchain, imports the window's
`AHardwareBuffer`, records and submits a Vulkan clear, presents it, then checks
the final pixels in an Android compositor screenshot.

```sh
ANDROID_SERIAL=<serial> examples/vulkan-probe/install-and-run.sh
```

Set `BIONICX_SCREENSHOT=path.png` to retain the screenshot.
