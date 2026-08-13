# Vortek multi-image swapchain lifetime

The X-window swapchain advertised `minImageCount=1` and always returned
image index 0. Chrome ANGLE and a resized window therefore never exercised
acquire rotation or recreate. A size mismatch returned
`VK_ERROR_SURFACE_LOST_KHR`, which clients treat differently from
`VK_ERROR_OUT_OF_DATE_KHR`.

The host now advertises `minImageCount=2` and `maxImageCount=3`. Extra
swapchain indices alias one imported window `AHardwareBuffer` (a second
import of the same buffer can leave the first image blank). Acquire rotates
so a presented index is not immediately reissued, and the host reports
`VK_ERROR_OUT_OF_DATE_KHR` when the X window extent changes or the window is
unmapped (zero size). Recreate uses the client's `oldSwapchain`
destroy/create sequence.

The Vulkan loader failed to `dlopen("libvulkan_vortek.so")` because the
interposed runtime resolves bare SONAMEs from the preload object, not the
executable RUNPATH. The ICD manifest now names
`../../../lib/libvulkan_vortek.so` relative to the JSON, and `dlopen` also
searches `dirname(/proc/self/exe)/../lib`.

The controlled `vulkan-probe` client covers ICD soname load, rotation,
resize-outdated, recreate and a map/unmap foreground cycle. Run it untraced
after installing the rebuilt APK:

```sh
ANDROID_SERIAL=<serial> examples/vulkan-probe/install-and-run.sh
```
