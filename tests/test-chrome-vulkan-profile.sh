#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/chrome-vulkan.json"
if grep -F -- '--single-process' "$repo_dir/profiles/chrome-vulkan.json" >/dev/null; then
    echo "chrome vulkan must use a real renderer process" >&2
    exit 1
fi
test -f "$repo_dir/examples/chrome/webgl-fixture.html"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/chrome-example.json"
if grep -F -- '--single-process' "$repo_dir/profiles/chrome-example.json" >/dev/null; then
    echo "chrome baidu must use a real renderer process" >&2
    exit 1
fi
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/chrome-smoke.json"
test -x "$repo_dir/examples/chrome/open-example.sh"
test -x "$repo_dir/examples/chrome/assert-example-page.py"
if python3 "$repo_dir/examples/chrome/assert-example-page.py" \
        "$repo_dir/evidence/vivo-10AFA31610002QH/chrome-smoke.png"; then
    echo "chrome://gpu screenshot must not pass the baidu-page assert" >&2
    exit 1
fi
if grep -F -- '--use-angle=gl' "$repo_dir/profiles/chrome-smoke.json" >/dev/null; then
    echo "chrome smoke must use ANGLE Vulkan, not Gladio GL" >&2
    exit 1
fi
if grep -F -- '--single-process' "$repo_dir/profiles/chrome-smoke.json" >/dev/null; then
    echo "chrome smoke must use a real renderer process" >&2
    exit 1
fi
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
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/chrome-swiftshader.json"
echo "chrome vulkan profile keeps ANGLE/Vortek and the pinned deb: PASS"
