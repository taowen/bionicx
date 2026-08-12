# Vulkan graphics pipeline and draw

The host Vulkan probe previously proved discovery, swapchain import, transfer
clear, submission and presentation. Chrome's black ANGLE frame required a
controlled test of the graphics path rather than another application trace.

The AArch64 glibc probe now loads compiled SPIR-V vertex and fragment shaders,
creates an image view, render pass, framebuffer, pipeline layout and graphics
pipeline, then records `vkCmdBeginRenderPass`, `vkCmdBindPipeline`,
`vkCmdDraw(3, 1, 0, 0)` and `vkCmdEndRenderPass`. The draw submission signals
the present semaphore and does not call `vkQueueWaitIdle` before present.

On x300 `01408BH601027129`, all 31 client checks passed through the app-private
Vortek RPC and the native Adreno driver. Android screenshot analysis separately
found both the exact green clear and red triangle, including their expected
spatial extent:

```
BXTEST PASS host-vulkan-render-target view=0 renderPass=0 framebuffer=0
BXTEST PASS host-vulkan-shader-modules vertex=0 fragment=0
BXTEST PASS host-vulkan-graphics-pipeline layout=0 pipeline=0 handle=valid
BXTEST PASS host-vulkan-record-triangle status=0 background=26,191,64 triangle=230,20,10
BXSUMMARY host-vulkan passed=31 failed=0
BXTEST PASS host-vulkan-compositor pixels=174537 bounds=0,0-639,359 triangle=55756 triangleBounds=90,51-549,291 size=1920x1080
```

This rules out the basic shader-module, fixed graphics state, render-pass,
framebuffer, draw batching, queue submission and AHardwareBuffer presentation
chain as the cause of Chrome's remaining black frame. See
`evidence/vulkan-graphics-pipeline.log` and
`evidence/vulkan-graphics-pipeline.png`.
