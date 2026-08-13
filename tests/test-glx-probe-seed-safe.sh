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
grep -F 'passed=26' \
    "$repo_dir/examples/glx-probe/install-and-run.sh" >/dev/null
echo "glx-probe install stays on the shared seed: PASS"
