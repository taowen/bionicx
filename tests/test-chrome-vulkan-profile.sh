#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/chrome-vulkan.json"
grep -F '"vulkan"' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F -- '--no-sandbox' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
if grep -F -- '--single-process' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null; then
    echo "chrome vulkan must use a real renderer process" >&2
    exit 1
fi
grep -F -- '--use-angle=vulkan' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F 'webgl-fixture.html' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F -- '--hide-crash-restore-bubble' \
    "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
test -f "$repo_dir/examples/chrome/webgl-fixture.html"
grep -F 'WEBGL_OK' "$repo_dir/examples/chrome/webgl-fixture.html" >/dev/null
grep -F 'UNMASKED_RENDERER_WEBGL' \
    "$repo_dir/examples/chrome/webgl-fixture.html" >/dev/null
grep -F 'WEBGL_NOT_VULKAN' \
    "$repo_dir/examples/chrome/webgl-fixture.html" >/dev/null
grep -F 'SwiftShader' "$repo_dir/examples/chrome/webgl-fixture.html" >/dev/null
grep -F 'webgl-fixture.html' \
    "$repo_dir/examples/chrome/build-bundle.sh" >/dev/null
grep -F 'VK_ICD_FILENAMES' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F '23f5d27be6ad6f5d69c1c11b602d4ed25a8499cfdfa11c3ca479ad0b58285499' \
    "$repo_dir/packages/external-arm64.tsv" >/dev/null
grep -F '../../../lib/libvulkan_vortek.so' \
    "$repo_dir/examples/chrome/build-bundle.sh" >/dev/null
grep -F 'build-gladio.sh' "$repo_dir/examples/chrome/build-bundle.sh" >/dev/null
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/chrome-smoke.json"
grep -F -- '--disable-gpu-watchdog' \
    "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F '"CHROME_EXTRA_FLAGS": "--disable-crashpad-for-testing --disable-gpu-watchdog"' \
    "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F '"vulkan"' "$repo_dir/profiles/chrome-smoke.json" >/dev/null
grep -F -- '--use-angle=vulkan' "$repo_dir/profiles/chrome-smoke.json" >/dev/null
grep -F 'chrome://gpu' "$repo_dir/profiles/chrome-smoke.json" >/dev/null
grep -F 'about:blank' "$repo_dir/profiles/chrome-smoke.json" >/dev/null
grep -F 'open-gpu.sh' "$repo_dir/examples/chrome/install-and-run.sh" >/dev/null
grep -F "type 'chrome://gpu'" "$repo_dir/examples/chrome/open-gpu.sh" >/dev/null
grep -F 'ctrl-l' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'list-windows' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
if grep -F -- '--use-angle=gl' "$repo_dir/profiles/chrome-smoke.json" >/dev/null; then
    echo "chrome smoke must use ANGLE Vulkan, not Gladio GL" >&2
    exit 1
fi
if grep -F -- '--single-process' "$repo_dir/profiles/chrome-smoke.json" >/dev/null; then
    echo "chrome smoke must use a real renderer process" >&2
    exit 1
fi
grep -F '"CHROME_EXTRA_FLAGS": "--disable-crashpad-for-testing --disable-gpu-watchdog"' \
    "$repo_dir/profiles/chrome-smoke.json" >/dev/null
grep -F 'VK_ICD_FILENAMES' "$repo_dir/profiles/chrome-smoke.json" >/dev/null
if grep -F 'with_chrome_child_arguments' \
        "$repo_dir/native/runtime/fhs-exec.c" >/dev/null; then
    echo "fhs-exec must not special-case Chrome argv" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/chrome/install-and-run.sh" >/dev/null; then
    echo "chrome install must not replace the shared seed" >&2
    exit 1
fi
echo "chrome vulkan profile keeps ANGLE/Vortek and the pinned deb: PASS"
