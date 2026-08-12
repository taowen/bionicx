# Vulkan index and uniform descriptor path

Chrome's request trace showed frequent descriptor binding and indexed draws.
The controlled graphics probe now adds two independently mapped allocations:
a `VK_BUFFER_USAGE_INDEX_BUFFER_BIT` buffer containing three `uint16_t`
indices, and a `VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT` buffer containing the exact
fragment tint.

It creates a descriptor-set layout and pool, allocates and updates a uniform
descriptor, includes that layout in the graphics pipeline, then records
`vkCmdBindDescriptorSets`, `vkCmdBindIndexBuffer`, and
`vkCmdDrawIndexed(3, 1, 0, 0, 0)`. The fragment shader must read the uniform to
produce the target red, so an API-only success cannot satisfy the screenshot.

On x300 `01408BH601027129`, the ordinary app UID passed all 34 checks:

```
BXTEST PASS host-vulkan-index-uniform-upload index=0 indexBytes=6 uniform=0 uniformBytes=16
BXTEST PASS host-vulkan-uniform-descriptor layout=0 pool=0 allocate=0 set=valid
BXTEST PASS host-vulkan-record-indexed-descriptor status=0 background=26,191,64 triangle=230,20,10
BXSUMMARY host-vulkan passed=34 failed=0
BXTEST PASS host-vulkan-compositor pixels=174537 bounds=0,0-639,359 triangle=55756 triangleBounds=90,51-549,291 size=1920x1080
```

This rules out the ordinary uniform descriptor and indexed-buffer chain as the
remaining cause of Chrome's incomplete frame. See
`evidence/vulkan-index-uniform-descriptor.log` and
`evidence/vulkan-index-uniform-descriptor.png`.
