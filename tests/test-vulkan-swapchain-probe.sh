#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
probe_dir="$repo_dir/examples/vulkan-probe"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-wsi.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-present.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-lifetime.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-frames.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-probe.json"
grep -F 'minImageCount >= 2' "$probe_dir"/*.[ch] >/dev/null
for name in vulkan-wsi-loader vulkan-wsi-window vulkan-wsi-device \
            vulkan-wsi-present-support vulkan-wsi-surface \
            vulkan-present-swapchain vulkan-present-pipeline vulkan-present \
            vulkan-frames vulkan-lifetime; do
    grep -F "$name" "$probe_dir"/*.[ch] >/dev/null
done
test -f "$probe_dir/vulkan-wsi.c"
test -f "$probe_dir/vulkan-present.c"
test -f "$probe_dir/vulkan-lifetime.c"
test -f "$probe_dir/vulkan-frames.c"
test -f "$probe_dir/assert-frames.py"
test ! -f "$probe_dir/vulkan-probe.c"
grep -Fq 'passed=5' "$probe_dir/install-and-run.sh"
grep -Fq 'passed=3' "$probe_dir/install-and-run.sh"
grep -Fq 'passed=1' "$probe_dir/install-and-run.sh"
grep -Fq 'background2=20,80,240' "$probe_dir/vulkan-frames.c"
grep -Fq '2048' "$probe_dir/vulkan-frames.c"
grep -F 'VK_SEMAPHORE_TYPE_TIMELINE' "$probe_dir/vulkan-frames.c" >/dev/null
grep -F 'hung after timeline wait plus present burst' \
    "$probe_dir/vulkan-frames.c" >/dev/null
grep -Fq 'host-vulkan-frames' "$probe_dir/assert-frames.py"
grep -Fq 'green < 20_000' "$probe_dir/assert-frames.py"
grep -F 'preferred_format' "$probe_dir"/*.[ch] >/dev/null
grep -F 'vkCmdBindVertexBuffers2' "$probe_dir"/*.[ch] >/dev/null
grep -F 'red_mask == 0xff0000UL' "$probe_dir"/*.[ch] >/dev/null
grep -F 'VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME' \
    "$probe_dir"/*.[ch] >/dev/null
if grep -F 'selected_format = formats[0]' "$probe_dir"/*.[ch] >/dev/null; then
    echo "swapchain probe must not fall back away from BGRA" >&2
    exit 1
fi
grep -F '../../../lib/libvulkan_vortek.so' "$probe_dir/build-bundle.sh" >/dev/null
grep -F 'compile vulkan-wsi' "$probe_dir/build-bundle.sh" >/dev/null
grep -F 'compile vulkan-present' "$probe_dir/build-bundle.sh" >/dev/null
grep -F 'compile vulkan-lifetime' "$probe_dir/build-bundle.sh" >/dev/null
grep -F 'compile vulkan-frames' "$probe_dir/build-bundle.sh" >/dev/null
grep -F -- '--app-root' "$probe_dir/install-and-run.sh" >/dev/null
if grep -F -- '--runtime-root' "$probe_dir/install-and-run.sh" >/dev/null; then
    echo "vulkan-probe install must not replace the shared seed" >&2
    exit 1
fi
grep -F 'return 2' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'VK_ERROR_OUT_OF_DATE_KHR' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'publishBuffer' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/include/xwindow_swapchain.h" \
    >/dev/null
grep -F 'publishMapped' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/include/xwindow_swapchain.h" \
    >/dev/null
if grep -F 'swapchain->images[i] = swapchain->images[0]' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
        >/dev/null; then
    echo "AHB present target must not alias client swapchain images" >&2
    exit 1
fi
grep -F 'AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE' \
    "$repo_dir/android/app/src/main/cpp/winlator/src/gpu_image.c" >/dev/null
grep -F 'AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT' \
    "$repo_dir/android/app/src/main/cpp/winlator/src/gpu_image.c" >/dev/null
grep -F 'AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN' \
    "$repo_dir/android/app/src/main/cpp/winlator/src/gpu_image.c" >/dev/null
grep -F 'unlockHardwareBuffer' \
    "$repo_dir/android/app/src/main/java/com/winlator/renderer/GPUImage.java" >/dev/null
grep -F 'GL_UNPACK_ROW_LENGTH' \
    "$repo_dir/android/app/src/main/java/com/winlator/renderer/GPUImage.java" >/dev/null
grep -F 'VK_FORMAT_R8G8B8A8_UNORM' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'VK_FORMAT_B8G8R8A8_UNORM' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'vkCmdBlitImage' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'createDeviceImage' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'VK_IMAGE_LAYOUT_PRESENT_SRC_KHR' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'presentBarrier' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'VK_ACCESS_HOST_READ_BIT' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F '80, 240' "$probe_dir"/*.[ch] >/dev/null
grep -F 'vulkan-present status=0' "$probe_dir/install-and-run.sh" >/dev/null
grep -F 'timeline-present' "$probe_dir"/*.[ch] >/dev/null
grep -F 'VK_SEMAPHORE_TYPE_TIMELINE' "$probe_dir"/*.[ch] >/dev/null
grep -F 'bin/vulkan-present' "$repo_dir/profiles/vulkan-probe.json" >/dev/null
grep -F 'Do not vkQueueWaitIdle here' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/request_handler.c" \
    >/dev/null
grep -F 'pending_fence_fd' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/request_handler.c" \
    >/dev/null
grep -F 'waitFenceThread' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/request_handler.c" \
    >/dev/null
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
grep -F 'vulkan-lifetime-deferred-fence' "$probe_dir/vulkan-lifetime.c" >/dev/null
grep -F 'submit_deferred_fence' "$probe_dir/vulkan-lifetime.c" >/dev/null
if grep -F 'vulkanWrapper.vkQueueWaitIdle' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
        >/dev/null; then
    echo "swapchain present must not vkQueueWaitIdle" >&2
    exit 1
fi
grep -F 'blitInFlight' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/include/xwindow_swapchain.h" \
    >/dev/null
grep -F 'pendingIndex' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/include/xwindow_swapchain.h" \
    >/dev/null
grep -F 'flush_pending_blit' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
if grep -F 'vkAllocateCommandBuffers(swapchain->device' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
        >/dev/null; then
    echo "present must not allocate a command buffer every frame" >&2
    exit 1
fi
grep -F 'vulkan-frames.json' "$probe_dir/install-and-run.sh" >/dev/null
grep -F 'assert-frames.py' "$probe_dir/install-and-run.sh" >/dev/null
echo "vulkan probes split into wsi/present/frames/lifetime: PASS"
