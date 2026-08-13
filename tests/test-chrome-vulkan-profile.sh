#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/chrome-vulkan.json"
grep -F '"vulkan"' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F -- '--no-sandbox' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F -- '--single-process' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F -- '--use-angle=vulkan' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F 'webgl-fixture.html' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F -- '--hide-crash-restore-bubble' \
    "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
test -f "$repo_dir/examples/chrome/webgl-fixture.html"
grep -F 'WEBGL_OK' "$repo_dir/examples/chrome/webgl-fixture.html" >/dev/null
grep -F 'webgl-fixture.html' \
    "$repo_dir/examples/chrome/build-bundle.sh" >/dev/null
grep -F 'VK_ICD_FILENAMES' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null
grep -F '23f5d27be6ad6f5d69c1c11b602d4ed25a8499cfdfa11c3ca479ad0b58285499' \
    "$repo_dir/packages/external-arm64.tsv" >/dev/null
grep -F '../../../lib/libvulkan_vortek.so' \
    "$repo_dir/examples/chrome/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/chrome/install-and-run.sh" >/dev/null; then
    echo "chrome install must not replace the shared seed" >&2
    exit 1
fi
echo "chrome vulkan profile keeps ANGLE/Vortek and the pinned deb: PASS"
