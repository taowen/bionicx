#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vulkan-probe.json"
grep -F 'minImageCount >= 2' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'host-vulkan-icd-library' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'host-vulkan-swapchain-acquire-rotate' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'host-vulkan-swapchain-resize-outdated' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'host-vulkan-swapchain-recreate' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'host-vulkan-swapchain-foreground' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'passed=40' \
    "$repo_dir/examples/vulkan-probe/install-and-run.sh" >/dev/null
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
grep -F 'swapchain->images[i] = swapchain->images[0]' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
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
grep -F 'VK_IMAGE_LAYOUT_PRESENT_SRC_KHR' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F 'VK_ACCESS_HOST_READ_BIT' \
    "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
    >/dev/null
grep -F '80, 240' \
    "$repo_dir/examples/vulkan-probe/vulkan-probe.c" >/dev/null
grep -F 'host-vulkan-present status=0' \
    "$repo_dir/examples/vulkan-probe/install-and-run.sh" >/dev/null
if grep -F 'VK_FORMAT_B8G8R8A8_UNORM' \
        "$repo_dir/android/app/src/main/cpp/vortekrenderer/src/xwindow_swapchain.c" \
        >/dev/null; then
    echo "Mali cannot import BGRA AHB as a color attachment" >&2
    exit 1
fi
echo "vulkan swapchain probe and host minImageCount=2: PASS"
