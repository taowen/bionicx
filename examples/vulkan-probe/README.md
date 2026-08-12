# Host Vulkan probe

This genuine AArch64 glibc client loads the pinned BionicX Vortek ICD through
the ordinary Linux Vulkan loader. Vortek transports Vulkan calls over an
app-private Unix socket to the Bionic Android host, which loads the system
Vulkan loader and the device's native vendor driver.

The probe verifies loader and instance discovery, instance creation,
physical-device identity, queue families, device memory, and the complete
Xlib WSI control plane: extension advertisement, real X window binding,
presentation support, capabilities, formats, and present modes. Command
submission and `AHardwareBuffer` presentation are the next controlled stage.

```sh
ANDROID_SERIAL=<serial> examples/vulkan-probe/install-and-run.sh
```
