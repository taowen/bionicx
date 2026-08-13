#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runner="$repo_dir/examples/dpkg-consistency/run.sh"
test -s "$repo_dir/packages/trixie-popular.txt"
test -s "$repo_dir/packages/external-arm64.tsv"
grep -F 'trixie-popular.txt' "$runner" >/dev/null
grep -F 'external-arm64.tsv' "$runner" >/dev/null
grep -F 'install --reinstall' "$runner" >/dev/null
grep -F 'remove ristretto' "$runner" >/dev/null
grep -F 'dpkg --audit' "$runner" >/dev/null
grep -F 'install ok installed' "$runner" >/dev/null
grep -F 'libc.so.6' "$runner" >/dev/null
grep -F 'cups-daemon' "$runner" >/dev/null
grep -F 'libtiff5' "$repo_dir/packages/external-arm64.tsv" >/dev/null
grep -F 'libwebp6' "$repo_dir/packages/external-arm64.tsv" >/dev/null
if grep -F -- '--runtime-root' "$runner" >/dev/null; then
    echo "dpkg-consistency must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$runner"
echo "dpkg consistency runner covers audit, reinstall and remove: PASS"
