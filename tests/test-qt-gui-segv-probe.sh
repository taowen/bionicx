#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/qt-gui-segv-probe.json"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/qt-gui-segv-probe/install-and-run.sh" \
        >/dev/null; then
    echo "qt-gui-segv-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/qt-gui-segv-probe/install-and-run.sh"
echo "qt gui segv probe covers XTEST, autotype and post-makeCurrent draw: PASS"
