#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
log="$repo_dir/evidence/rebuild-2026-08-13/bxapt-reconstruct.log"
test -s "$log"
grep -F 'ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2' \
    "$log" >/dev/null
grep -F 'tools/bxapt' "$repo_dir/docs/NEW-DEVICE.md" >/dev/null
grep -F 'packages/trixie-popular.txt' "$repo_dir/docs/NEW-DEVICE.md" >/dev/null
while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line%%#*}"
    read -r package extra <<< "$line"
    [[ -n "${package:-}" ]] || continue
    grep -F "$package" "$repo_dir/packages/trixie-popular.txt" >/dev/null
    grep -E "$package"'[[:space:]]*install ok installed' "$log" >/dev/null
done < "$repo_dir/packages/trixie-popular.txt"
grep -F 'google-chrome-stable' "$log" >/dev/null
grep -F '151.0.7922.108-1' "$log" >/dev/null
grep -F '23f5d27be6ad6f5d69c1c11b602d4ed25a8499cfdfa11c3ca479ad0b58285499' \
    "$log" >/dev/null
grep -F 'wps-office' "$log" >/dev/null
grep -F '11.1.0.11720' "$log" >/dev/null
grep -F '172d8bcf3e3bc756994ae5adf66f63f22691e1ab0d18986c50bb6b6ab7f62948' \
    "$log" >/dev/null
grep -F 'qt6-qpa-plugins' "$log" >/dev/null
grep -F 'The following packages will be REMOVED:' "$log" >/dev/null
grep -F 'ristretto' "$log" >/dev/null
grep -F 'ristretto install ok installed 0.13.3-1' "$log" >/dev/null
grep -F '(end find)' "$log" >/dev/null
if grep -E 'files/apps/.*/libc.so.6' "$log" >/dev/null; then
    echo "reconstruct log must not list a per-app libc.so.6" >&2
    exit 1
fi
echo "bxapt reconstruct evidence covers seed, declarations and empty audit: PASS"
