# Host Vulkan probes

Three independent glibc clients share the pinned Vortek ICD and a small
bring-up helper. Each probe is a complete vertical slice:

| Probe | Binary | What it proves |
|---|---|---|
| `vulkan-wsi` | `bin/vulkan-wsi` | ICD/loader, X11 BGRA visual, physical device honesty, Xlib+XCB WSI, surface BGRA/FIFO |
| `vulkan-present` | `bin/vulkan-present` | BGRA swapchain, graphics pipeline, `SetViewportWithCount`/`SetScissorWithCount`/`BindVertexBuffers2`, present + Chrome timeline handshake, compositor pixels |
| `vulkan-frames` | `bin/vulkan-frames` | green then blue after a timeline wait + present burst |
| `vulkan-chrome-frames` | `bin/vulkan-chrome-frames` | 1920×1080 overlapping ANGLE acquire(sem+fence) + present; first green then blue |
| `vulkan-lifetime` | `bin/vulkan-lifetime` | acquire rotate, resize `OUT_OF_DATE`, recreate, unmap/remap |
| `vulkan-bcn` | `bin/vulkan-bcn` | BC1 must be advertised and creatable; BC7 create matches format advertisement; timeline `CreateDevice`, `GetSemaphoreCounterValue`, and `QueueSubmit` signal |

`install-and-run.sh` builds the probes and runs them in that order. The
ICD JSON names `../../../lib/libvulkan_vortek.so`. The payload installs
against the existing seed rootfs and does not replace `/files/rootfs`.

```sh
ANDROID_SERIAL=<serial> examples/vulkan-probe/install-and-run.sh
```

Set `BIONICX_SCREENSHOT=path.png` to retain the present-probe screenshot.
A single probe can be launched with `profiles/vulkan-wsi.json`,
`profiles/vulkan-present.json`, or `profiles/vulkan-lifetime.json`.
`profiles/vulkan-probe.json` still launches the present probe.
