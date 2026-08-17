#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/qt-glx-fbconfig-probe.json"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/qt-glx-fbconfig-probe/install-and-run.sh" \
        >/dev/null; then
    echo "qt-glx-fbconfig-probe must not replace the shared seed" >&2
    exit 1
fi
echo "qt glx fbconfig probe requires a SingleBuffer config for Qt: PASS"
