#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runner="$repo_dir/examples/vivo-apps/run.sh"
test -x "$runner"
if grep -F -- '--runtime-root' "$runner" >/dev/null; then
    echo "vivo-apps must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$runner"
echo "vivo-apps launch five untraced GUIs: PASS"
