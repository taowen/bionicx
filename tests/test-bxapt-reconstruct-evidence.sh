#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
log="$repo_dir/evidence/rebuild-2026-08-13/bxapt-reconstruct.log"
test -s "$log"
if grep -E 'files/apps/.*/libc.so.6' "$log" >/dev/null; then
    echo "reconstruct log must not list a per-app libc.so.6" >&2
    exit 1
fi
echo "bxapt reconstruct evidence covers seed, declarations and empty audit: PASS"
