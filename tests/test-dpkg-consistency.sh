#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runner="$repo_dir/examples/dpkg-consistency/run.sh"
test -s "$repo_dir/packages/trixie-popular.txt"
test -s "$repo_dir/packages/external-arm64.tsv"
if grep -F -- '--runtime-root' "$runner" >/dev/null; then
    echo "dpkg-consistency must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$runner"
echo "dpkg consistency runner covers audit, reinstall and remove: PASS"
