# Host Vulkan probe

This genuine AArch64 glibc client loads the pinned BionicX Vortek ICD through
the ordinary Linux Vulkan loader. Vortek transports Vulkan calls over an
app-private Unix socket to the Bionic Android host, which loads the system
Vulkan loader and the device's native vendor driver.

The first integration stage verifies loader and instance discovery, instance
creation, physical-device identity, queue families, and device memory. Xlib
surface creation, command submission, and `AHardwareBuffer` presentation are
the next controlled stages.

```sh
ANDROID_SERIAL=<serial> examples/vulkan-probe/install-and-run.sh
```
