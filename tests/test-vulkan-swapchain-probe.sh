#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
probe_dir="$repo_dir/examples/vulkan-probe"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-wsi.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-present.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-lifetime.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-frames.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-bcn.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-probe.json"
test -f "$probe_dir/vulkan-wsi.c"
test -f "$probe_dir/vulkan-present.c"
test -f "$probe_dir/vulkan-lifetime.c"
test -f "$probe_dir/vulkan-frames.c"
test -f "$probe_dir/vulkan-bcn.c"
test -f "$probe_dir/assert-frames.py"
test ! -f "$probe_dir/vulkan-probe.c"
if grep -F 'selected_format = formats[0]' "$probe_dir"/*.[ch] >/dev/null; then
    echo "swapchain probe must not fall back away from BGRA" >&2
    exit 1
fi
if grep -F -- '--runtime-root' "$probe_dir/install-and-run.sh" >/dev/null; then
    echo "vulkan-probe install must not replace the shared seed" >&2
    exit 1
fi
if grep -F 'swapchain->images[i] = swapchain->images[0]' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
        >/dev/null; then
    echo "AHB present target must not alias client swapchain images" >&2
    exit 1
fi
if grep -F '&fences[i]' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/request_handler.c" \
        | grep -F vkWaitForFences >/dev/null; then
    echo "WaitForFences must not block the RPC thread on timeout != 0" >&2
    exit 1
fi
if awk '/void vt_handle_vkWaitForFences/,/^void vt_handle_vkCreateSemaphore/' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/request_handler.c" \
        | grep -F vkGetFenceFd >/dev/null; then
    echo "WaitForFences must not export SYNC_FD; that resets the fence" >&2
    exit 1
fi
if grep -F 'vulkanWrapper.vkQueueWaitIdle' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
        >/dev/null; then
    echo "swapchain present must not vkQueueWaitIdle" >&2
    exit 1
fi
if grep -F 'updateWindowContent' \
        "$repo_dir/android/app/src/main/java/com/winlator/xenvironment/components/VortekRendererComponent.java" \
        >/dev/null; then
    echo "Vortek present path must not keep unused updateWindowContent" >&2
    exit 1
fi
if grep -E 'swapchain->presented|int presented|XWindowSwapchain_presentImage\(' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/include/xwindow_swapchain.h" \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
        >/dev/null; then
    echo "unused presented/presentImage leftovers must stay gone" >&2
    exit 1
fi
if grep -F 'vkAllocateCommandBuffers(swapchain->device' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
        >/dev/null; then
    echo "present must not allocate a command buffer every frame" >&2
    exit 1
fi
if grep -F 'vt_send(context->clientRing, result, &semaphore, sizeof(uint64_t))' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/request_handler.c" \
        >/dev/null; then
    echo "GetSemaphoreCounterValue must send the counter, not the handle" >&2
    exit 1
fi
echo "vulkan probes split into wsi/present/frames/lifetime: PASS"
