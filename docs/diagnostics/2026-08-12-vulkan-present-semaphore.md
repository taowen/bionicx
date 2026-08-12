# Vulkan present semaphore ownership

The original X-window swapchain path deserialized `VkPresentInfoKHR`, but
discarded both the presenting queue and `pWaitSemaphores`. It notified the
Android compositor immediately. That races an asynchronous ANGLE submission
and can expose an incomplete `AHardwareBuffer` frame.

The glibc Vortek ICD now serializes the queue handle with the generated
`vkQueuePresentKHR` command. The Bionic renderer resolves that real host queue,
submits an empty wait for all present semaphores, waits for completion, and
only then calls `updateWindowContent`.

The controlled probe no longer calls `vkQueueWaitIdle` before present. It
creates a binary semaphore, signals it from the clear submission, and passes
it only through `VkPresentInfoKHR`. Thus the final screenshot can succeed only
if the cross-process present path consumes the semaphore.

On x300 `01408BH601027129`, as the ordinary `io.taowen.bx` app UID, all 28
client checks passed and Android found the exact rendered window:

```
BXTEST PASS host-vulkan-present-semaphore status=0 handle=valid
BXTEST PASS host-vulkan-submit-clear submit=0 signal=present-semaphore
BXTEST PASS host-vulkan-present status=0 index=0 color=26,191,64,255
BXSUMMARY host-vulkan passed=28 failed=0
BXTEST PASS host-vulkan-compositor pixels=230293 bounds=0,0-639,359 size=1920x1080
```

See `evidence/vulkan-present-semaphore.log` and
`evidence/vulkan-present-semaphore.png`.
