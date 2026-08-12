# Vulkan sampled-image upload

The controlled probe now covers Chrome's remaining basic texture path. It maps
a staging buffer in the glibc process, uploads a 2x2 RGBA texture, creates an
optimal-tiled device-local image, memory, view and nearest sampler, then
records the required layout transitions and `vkCmdCopyBufferToImage`.

The descriptor set contains both the existing uniform buffer and a
`VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`. The fragment shader samples the
texture and multiplies it into the exact target color. Therefore the final red
triangle proves staging visibility, image allocation/binding, copy batching,
barriers, descriptor update/binding and shader sampling together.

On x300 `01408BH601027129`, the ordinary app UID passed 35/35:

```
BXTEST PASS host-vulkan-sampled-image staging=0 image=0 type=0 allocate=0 bind=0 view=0 sampler=0
BXTEST PASS host-vulkan-uniform-descriptor layout=0 pool=0 allocate=0 set=valid
BXSUMMARY host-vulkan passed=35 failed=0
BXTEST PASS host-vulkan-compositor pixels=174537 bounds=0,0-639,359 triangle=55756 triangleBounds=90,51-549,291 size=1920x1080
```

The basic sampled-image chain is no longer a candidate for Chrome's incomplete
frame. See `evidence/vulkan-sampled-image.log` and
`evidence/vulkan-sampled-image.png`.
