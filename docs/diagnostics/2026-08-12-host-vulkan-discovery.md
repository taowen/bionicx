# glibc Vulkan discovery through the Android vendor driver

BionicX now starts a Vulkan host service only for profiles that declare
`hostServices: ["vulkan"]`. A genuine AArch64 glibc program loads Debian's
ordinary Vulkan loader and the pinned BionicX Vortek ICD. The ICD connects to
an app-private Unix socket and exchanges requests through shared ring buffers
with a Bionic server inside the APK. That server loads Android's system Vulkan
loader, which selects the device vendor HAL.

This boundary is required for proprietary Android drivers: the glibc process
cannot safely load a Bionic vendor driver and its Android-private dependencies
into the same address space. It also gives the later Xlib WSI implementation a
place to translate X window IDs into BionicX `AHardwareBuffer` swapchains.

The Vortek client is pinned as `third_party/vortek` rather than copied into the
repository. The small Bionic server remains integrated with the Winlator-
derived Android/X-server source because it directly calls Java window and
`GPUImage` objects. BionicX removes the optional custom-AdrenoTools path for
this baseline and always loads `/system/lib64/libvulkan.so`; that supports the
device's stock Adreno or Mali driver without shipping vendor binaries.

## Controlled verification

The first probe deliberately stops before surface creation. On x300
`01408BH601027129`, under the ordinary application UID and without tracing:

```text
BXTEST PASS host-vulkan-loader status=0 version=1.4.309
BXTEST PASS host-vulkan-instance-extensions status=0 extensions=16
BXTEST PASS host-vulkan-create-instance status=0 handle=valid
BXTEST PASS host-vulkan-physical-devices status=0 devices=1
BXTEST PASS host-vulkan-device-properties status=0 name=Vortek (Adreno (TM) 750) api=1.3.128 vendor=0x5143 device=0x43051401
BXTEST PASS host-vulkan-queue-families families=3
BXTEST PASS host-vulkan-memory types=9 heaps=2 bytes=9388290048
BXSUMMARY host-vulkan passed=7 failed=0
```

Android logs independently identify the selected implementation as
`/vendor/lib64/hw/vulkan.adreno.so`. The next controlled stage will create a
real Xlib surface and validate surface capabilities before adding command
submission and visible `AHardwareBuffer` presentation.
