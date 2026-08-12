# Chrome Vulkan ignored tessellation state

Chrome ANGLE reached `vkCreateGraphicsPipelines`, but the Adreno host driver
printed `Unknown tessellation state create type: 0` for every ordinary
vertex/fragment pipeline.

Temporary diagnostics on both sides of the Vortek RPC established that the
wire serializer was not corrupting this field. The glibc client supplied a
non-null `pTessellationState` whose `sType` was already zero, and the Bionic
renderer reconstructed the same value:

```
VortekClient graphics[0] sType=28 stages=0x1 tess=0x... tessSType=0
VortekPipeline graphics[0] sType=28 stages=0x1 tess=0x... tessSType=0
AdrenoVK-0: Unknown tessellation state create type: 0
```

Vulkan does not use `pTessellationState` when the pipeline has no tessellation
control or evaluation shader stage. Before calling the host driver, the
renderer now derives that condition from every `pStages` entry and sets the
ignored pointer to `NULL`. It does not invent a structure type, and it leaves
real tessellation pipelines unchanged.

An untraced Chrome `--use-angle=vulkan --no-sandbox` run on x300 then kept the
app and browser processes alive with zero Adreno tessellation warnings and
zero fatal/GPU-exit messages. Full Chrome Vulkan frame rendering is still not
correct; this change removes one diagnosed driver-interface violation rather
than claiming the remaining black frame is solved.

See `evidence/chrome-vulkan-tessellation-state.log`.
