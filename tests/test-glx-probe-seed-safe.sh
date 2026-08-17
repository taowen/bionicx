#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/glx-probe.json"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/glx-probe/install-and-run.sh" >/dev/null; then
    echo "glx-probe install must not replace the shared seed" >&2
    exit 1
fi
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
echo "glx-probe install stays on the shared seed: PASS"
