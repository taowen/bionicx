# Cross-process Vulkan mapped vertex upload

The first controlled graphics probe used `gl_VertexIndex`, so it did not cover
the resource-upload path that dominates Chrome. The upgraded probe puts three
positions and RGB colors in an interleaved host-visible vertex buffer using
`vkAllocateMemory`, `vkBindBufferMemory`, `vkMapMemory`, a glibc `memcpy`,
`vkUnmapMemory`, and `vkCmdBindVertexBuffers`.

All API calls initially returned `VK_SUCCESS`, but the Android screenshot was
entirely black. This reproduced Chrome's symptom with 60 bytes of controlled
input. Vortek mapped the exported Android allocation FD and immediately closed
it. It did not bracket CPU access with the synchronization required by the
Linux dma-buf userspace ABI, so the host GPU did not reliably observe glibc's
writes.

The Vortek client now retains the FD for the mapping lifetime, calls
`DMA_BUF_IOCTL_SYNC` with `START|RW` before CPU access, and `END|RW` after
`msync`/`munmap`, then closes it. An opaque external-memory FD that does not
implement the dma-buf ioctl safely retains the old mapping behavior.

On x300 `01408BH601027129`, the same unrooted test then passed 32/32 and the
Android compositor recovered the exact two-color triangle:

```
BXTEST PASS host-vulkan-vertex-upload create=0 type=6 allocate=0 bind=0 map=0 bytes=60
BXSUMMARY host-vulkan passed=32 failed=0
BXTEST PASS host-vulkan-compositor pixels=174537 bounds=0,0-639,359 triangle=55756 triangleBounds=90,51-549,291 size=1920x1080
```

See `evidence/vulkan-vertex-upload-before.png`,
`evidence/vulkan-vertex-upload-after.png`, and
`evidence/vulkan-vertex-upload.log`.
