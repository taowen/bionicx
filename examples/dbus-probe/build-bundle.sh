#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/dbus-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir/app"
printf 'required_package=dbus\nrootfs_payload=none\n' \
    > "$output_dir/BUILD-INFO"
printf '%s\n' "$output_dir"
