#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-probe.json"
grep -F 'minImageCount >= 2' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
for name in host-vulkan-loader host-vulkan-window host-vulkan-device \
            host-vulkan-wsi host-vulkan-surface host-vulkan-swapchain \
            host-vulkan-pipeline host-vulkan-present \
            host-vulkan-swapchain-lifetime; do
    grep -F "$name" "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
done
grep -Fq 'passed=9' "$repo_dir/examples/vulkan-probe/install-and-run.sh"
# Fragmented one-bug-one-name checks stay folded into the nine stages.
for name in host-vulkan-icd-library host-vulkan-angle-bgra \
            host-vulkan-root-visual host-vulkan-x11-bgra-visual \
            host-vulkan-swapchain-acquire-rotate \
            host-vulkan-swapchain-resize-outdated \
            host-vulkan-swapchain-recreate \
            host-vulkan-swapchain-foreground; do
    if grep -F "result(\"$name\"" \
            "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null; then
        echo "stale fragmented probe name still reported: $name" >&2
        exit 1
    fi
done
grep -F 'preferred_format' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'vkCmdBindVertexBuffers2' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'red_mask == 0xff0000UL' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
if grep -F 'selected_format = formats[0]' \
        "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null; then
    echo "swapchain probe must not fall back away from BGRA" >&2
    exit 1
fi
grep -F '../../../lib/libvulkan_vortek.so' \
    "$repo_dir/examples/vulkan-probe/build-bundle.sh" >/dev/null
grep -F -- '--app-root' \
    "$repo_dir/examples/vulkan-probe/install-and-run.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/vulkan-probe/install-and-run.sh" >/dev/null; then
    echo "vulkan-probe install must not replace the shared seed" >&2
    exit 1
fi
grep -F 'return 2' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'VK_ERROR_OUT_OF_DATE_KHR' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'presentTarget' \
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
grep -F '80, 240' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'host-vulkan-present status=0' \
    "$repo_dir/examples/vulkan-probe/install-and-run.sh" >/dev/null
grep -F 'timeline-present' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'VK_SEMAPHORE_TYPE_TIMELINE' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'Do not vkQueueWaitIdle here' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/request_handler.c" \
    >/dev/null
if grep -F 'vulkanWrapper.vkQueueWaitIdle' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
        >/dev/null; then
    echo "swapchain present must not vkQueueWaitIdle" >&2
    exit 1
fi
echo "vulkan swapchain probe and host minImageCount=2: PASS"
