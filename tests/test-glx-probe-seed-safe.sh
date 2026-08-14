#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/glx-probe.json"
grep -F -- '--app-root' \
    "$repo_dir/examples/glx-probe/install-and-run.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/glx-probe/install-and-run.sh" >/dev/null; then
    echo "glx-probe install must not replace the shared seed" >&2
    exit 1
fi
grep -F 'passed=5' \
    "$repo_dir/examples/glx-probe/install-and-run.sh" >/dev/null
grep -F 'glx-setup' "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null
grep -F 'glx-config' "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null
grep -F 'host-gl-capabilities' \
    "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null
if grep -F 'result("glx-display"' \
        "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null; then
    echo "stale fragmented glx-display result remains" >&2
    exit 1
fi
if grep -F 'result("host-gl-identity"' \
        "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null; then
    echo "stale fragmented host-gl-identity result remains" >&2
    exit 1
fi
grep -F 'MapNotify' \
    "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null
grep -F 'awaitEglContext' \
    "$repo_dir/android/app/src/main/java/com/winlator/renderer/GLRenderer.java" \
    >/dev/null
grep -F 'deadline.tv_sec += 2' \
    "$repo_dir/android/app/src/main/cpp/gladiorenderer/src/gl_context.c" \
    >/dev/null
grep -F 'requestRender' \
    "$repo_dir/android/app/src/main/java/com/winlator/BionicXActivity.java" \
    >/dev/null
grep -F 'retry without share' \
    "$repo_dir/android/app/src/main/cpp/gladiorenderer/src/gl_context.c" \
    >/dev/null
grep -F 'fbconfig_count == 3' \
    "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null
grep -F 'GLX_VISUAL_ID' \
    "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null
grep -F 'GLX_X_RENDERABLE' \
    "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null
grep -F 'expected_ids' \
    "$repo_dir/examples/glx-probe/glx-probe.c" >/dev/null
grep -F 'GLX_VISUAL_ID' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/GLXExtension.java" \
    >/dev/null
grep -F 'GLX_X_RENDERABLE' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/GLXExtension.java" \
    >/dev/null
grep -F 'SINGLE_FBCONFIG_ID' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/GLXExtension.java" \
    >/dev/null
echo "glx-probe install stays on the shared seed: PASS"
