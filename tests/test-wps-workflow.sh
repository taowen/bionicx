#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/wps/run-workflows.sh" >/dev/null; then
    echo "wps workflows must not replace the shared seed" >&2
    exit 1
fi
echo "wps workflow runner covers save, clipboard, print and slideshow: PASS"
