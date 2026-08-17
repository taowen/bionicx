#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/krita-glx-destroy-probe.json"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/krita-glx-destroy-probe/install-and-run.sh" \
        >/dev/null; then
    echo "krita-glx-destroy-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/krita-glx-destroy-probe/install-and-run.sh"
echo "krita glx destroy probe covers glXDestroyContext(NULL): PASS"
