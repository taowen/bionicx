# Chrome ANGLE Vulkan device capability and Queue2 ownership

After XCB WSI, Chrome ANGLE reached device creation and exposed two independent
Vortek correctness bugs.

First, Vortek copied every host device extension into the guest enumeration
even when its RPC protocol did not implement the corresponding entry points.
The Adreno host advertises `VK_KHR_fragment_shading_rate`, but the Vortek ICD
does not implement `vkGetPhysicalDeviceFragmentShadingRatesKHR`. The glibc
loader therefore rejected the inconsistent ICD dispatch table before ANGLE
could create a device. BionicX now filters this extension for every engine. It
does not fake a shading-rate result; it truthfully lets ANGLE use its optional
fallback.

Second, the custom X-window swapchain called `vkGetDeviceQueue` internally.
ANGLE obtains its queue with `vkGetDeviceQueue2`; calling the old Android loader
entry point again with that device caused a null dispatch dereference in the
Bionic host process. The root tombstone made the ownership error explicit:

```text
#00 /system/lib64/libvulkan.so
    vulkan::driver::GetDeviceQueue(...)+88
#01 libvortekrenderer.so XWindowSwapchain_create+660
#02 libvortekrenderer.so vt_handle_vkCreateSwapchainKHR+1024
Cause: null pointer dereference
```

The Vulkan server now records the graphics queue returned by either
`vkGetDeviceQueue` or `vkGetDeviceQueue2` in the per-connection `VkContext`.
The swapchain reuses the queue the client actually acquired and no longer
performs a hidden second queue lookup.

## Verification

The controlled probe covers the old Queue path, capability filtering, full
swapchain submission, and final Android pixels:

```text
BXTEST PASS host-vulkan-device-extension-honesty status=0 returned=133 swapchain=1 fragmentShadingRate=0
BXTEST PASS host-vulkan-device-queue handle=valid family=0 index=0
BXTEST PASS host-vulkan-swapchain status=0 handle=valid format=44 extent=640x360 images=1
BXTEST PASS host-vulkan-present status=0 index=0 color=26,191,64,255
BXSUMMARY host-vulkan passed=27 failed=0
BXTEST PASS host-vulkan-compositor pixels=230293 bounds=0,0-639,359 size=1920x1080
```

In an untraced Chrome launch retaining `--no-sandbox`, ANGLE then created its
Adreno device and swapchain, the browser and Bionic host remained alive after
ten seconds, and the Android screenshot showed the full-screen tab/omnibox and
content surfaces. Several areas are still black and text is incomplete. Adreno
now repeatedly reports `Unknown tessellation state create type: 0`, moving the
next investigation to graphics-pipeline state serialization rather than WSI,
device, queue, or swapchain setup.
