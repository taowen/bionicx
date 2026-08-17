#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/wps-font-probe/install-and-run.sh" >/dev/null; then
    echo "wps-font-probe must not replace the shared seed" >&2
    exit 1
fi
echo "wps font probe checks Liberation coverage before MS aliases: PASS"
