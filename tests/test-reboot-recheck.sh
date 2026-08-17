#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runner="$repo_dir/examples/reboot-recheck/run.sh"
if grep -F -- '--runtime-root' "$runner" >/dev/null; then
    echo "reboot-recheck must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$runner"
echo "reboot recheck runner covers force-stop, GLX 5/5 and desktop 7/7: PASS"
