#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
temporary="$(mktemp -d)"
trap 'find "$temporary" -mindepth 1 -delete; rmdir "$temporary"' EXIT

printf '\177ELF' > "$temporary/client"
mock_readelf="$temporary/readelf"
cp "$repo_dir/tests/fixtures/mock-readelf.sh" "$mock_readelf"
chmod +x "$mock_readelf"

MOCK_GLIBC_VERSION=2.39 "$repo_dir/tools/check-glibc-symbol-floor.py" \
    "$temporary" --maximum 2.39 --readelf "$mock_readelf" \
    | grep -Fq 'checked 1 ELF objects'
if MOCK_GLIBC_VERSION=2.41 "$repo_dir/tools/check-glibc-symbol-floor.py" \
        "$temporary" --maximum 2.39 --readelf "$mock_readelf" >/dev/null 2>&1; then
    echo "newer GLIBC requirement was accepted" >&2
    exit 1
fi
echo "GLIBC symbol floor guard passes"
