# Vulkan BindVertexBuffers2 optional-array fidelity

Chrome records `vkCmdBindVertexBuffers2` hundreds of times during its first
frames. Vulkan requires `pBuffers` and `pOffsets` when `bindingCount` is
nonzero, but explicitly permits `pSizes` and `pStrides` to be `NULL`.

The controlled Vulkan probe now uses that exact form while retaining its
mapped vertex/index/uniform buffers, sampled image, descriptor set and indexed
draw:

```c
vkCmdBindVertexBuffers2(commandBuffer, 0, 1, &vertexBuffer,
                        &offset, NULL, NULL);
```

The wire serializer correctly encoded both absent arrays as zero-length. The
generated unserializer, however, assigned `NULL` only to its local pointer
parameters. The Android request handler then always passed its uninitialized
stack arrays to the vendor driver. Every Vulkan call still reported success,
but the compositor screenshot was completely black:

```
BXTEST FAIL host-vulkan-compositor pixels=0 bounds=-1,-1--1,-1 triangle=0 triangleBounds=-1,-1--1,-1 size=1920x1080
```

The request handler now reads the serialized array-presence fields with bounds
checks and preserves the two optional pointers as `NULL`. The same unrooted
glibc client on x300 `01408BH601027129` then passed 35/35 and recovered the
exact expected image:

```
BXTEST PASS host-vulkan-record-bind2-indexed-descriptor status=0 background=26,191,64 triangle=230,20,10
BXSUMMARY host-vulkan passed=35 failed=0
BXTEST PASS host-vulkan-compositor pixels=174537 bounds=0,0-639,359 triangle=55756 triangleBounds=90,51-549,291 size=1920x1080
```

See `evidence/vulkan-bind2-null-before.png`,
`evidence/vulkan-bind2-null-after.png`, and
`evidence/vulkan-bind2-null.log`.

## Chrome regression

The ordinary untraced Chrome Vulkan profile was then relaunched with its
required `--no-sandbox` argument. It remained alive, rendered the complete
1920x1080 browser frame with no black regions, accepted Android-injected
address-bar input, rendered its network error page, and opened `chrome://gpu`.
That page reports hardware-accelerated Canvas, compositing, rasterization,
WebGL and WebGPU. Its active GPU is:

```
ANGLE (Qualcomm, Vulkan 1.3.128 (Vortek (Adreno (TM) 750 ...)))
```

See `evidence/chrome-vulkan-bind2-fixed.png`,
`evidence/chrome-vulkan-gpu-bind2-fixed.png`, and
`evidence/chrome-vulkan-bind2-fixed.log`.
