#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
while IFS=$'\t' read -r package version sha256 url; do
    [[ "$package" == libwebp6 || "$package" == libtiff5 ]] || continue
    dest_dir="$repo_dir/build/cache/${package}-${version}"
    dest="$dest_dir/${url##*/}"
    mkdir -p "$dest_dir"
    if [[ -f "$dest" ]]; then
        actual="$(sha256sum "$dest" | cut -d' ' -f1)"
        if [[ "$actual" == "$sha256" ]]; then
            printf 'cached %s sha256=%s\n' "$dest" "$actual"
            continue
        fi
    fi
    curl --fail --location --retry 3 --output "$dest.partial" "$url"
    mv "$dest.partial" "$dest"
    actual="$(sha256sum "$dest" | cut -d' ' -f1)"
    if [[ "$actual" != "$sha256" ]]; then
        echo "hash mismatch for $dest" >&2
        echo "expected: $sha256" >&2
        echo "actual:   $actual" >&2
        exit 1
    fi
    printf 'fetched %s sha256=%s\n' "$dest" "$actual"
done < <(grep -E '^(libwebp6|libtiff5)' "$repo_dir/packages/external-arm64.tsv")
