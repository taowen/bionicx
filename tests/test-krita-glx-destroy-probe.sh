#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/krita-glx-destroy-probe.json"
grep -F 'glXDestroyContext(display, NULL)' \
    "$repo_dir/examples/krita-glx-destroy-probe/krita-glx-destroy-probe.c" \
    >/dev/null
grep -F 'destroy-null' \
    "$repo_dir/examples/krita-glx-destroy-probe/krita-glx-destroy-probe.c" \
    >/dev/null
grep -F 'glXCreateNewContext' \
    "$repo_dir/examples/krita-glx-destroy-probe/krita-glx-destroy-probe.c" \
    >/dev/null
grep -F 'if (!ctx) return' \
    "$repo_dir/third_party/gladio/src/glx_calls.c" >/dev/null
grep -F 'GLADIO_X11_SOCKET' \
    "$repo_dir/profiles/krita-glx-destroy-probe.json" >/dev/null
grep -F 'passed=4 failed=0' \
    "$repo_dir/examples/krita-glx-destroy-probe/install-and-run.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/krita-glx-destroy-probe/install-and-run.sh" \
        >/dev/null; then
    echo "krita-glx-destroy-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/krita-glx-destroy-probe/install-and-run.sh"
echo "krita glx destroy probe covers glXDestroyContext(NULL): PASS"
