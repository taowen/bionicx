# Chrome ANGLE Vulkan requires XCB WSI

Chrome 151 with Ozone X11 does not use the Xlib Vulkan surface entry points
from the first controlled probe. ANGLE loads the same glibc Vulkan loader and
Vortek ICD but requests `VK_KHR_xcb_surface`. Its first deterministic failure
was therefore extension negotiation, before device creation:

```text
VerifyExtensionsPresent: Extension not supported: VK_KHR_xcb_surface
eglInitialize Vulkan failed with error EGL_NOT_INITIALIZED
```

Xlib `Window` and XCB `xcb_window_t` are representations of the same 32-bit X
resource ID. The BionicX Vortek fork now exports
`vkCreateXcbSurfaceKHR` and
`vkGetPhysicalDeviceXcbPresentationSupportKHR`; the surface object retains that
XID and reuses the existing Java X-window-to-AHardwareBuffer swapchain. The
Bionic server advertises XCB WSI to the glibc client and strips it before
calling Android's native `vkCreateInstance`, just as it does for Xlib WSI.

## Controlled and Chrome verification

The controlled probe enables both surface extensions against one real X
window and still performs its full present plus Android screenshot assertion:

```text
BXTEST PASS host-vulkan-xcb-extension status=0 xcb=1
BXTEST PASS host-vulkan-xcb-surface status=0 handle=valid window=0x400001
BXTEST PASS host-vulkan-xcb-presentation-support family=0 xcb=1
BXSUMMARY host-vulkan passed=26 failed=0
BXTEST PASS host-vulkan-compositor pixels=230293 bounds=0,0-639,359 size=1920x1080
```

The separate `chrome-vulkan.json` profile retains `--no-sandbox`, selects
`--use-gl=angle --use-angle=vulkan`, and keeps Skia Graphite disabled while
this layer is qualified. After the XCB implementation, Chrome's ANGLE process
creates an instance whose engine is `ANGLE` through
`/vendor/lib64/hw/vulkan.adreno.so`. Its next failure is no longer WSI:

```text
Application Name    : chrome
Engine Name         : ANGLE
[Vulkan Loader] ERROR: ICD associated with VkPhysicalDevice does not support GetPhysicalDeviceFragmentShadingRatesKHR
GPU process exited unexpectedly: exit_code=6
```

That is the next narrow ICD-dispatch gap. The ordinary Chrome OpenGL profile
is unchanged and remains the working acceptance baseline.
