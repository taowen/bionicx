#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/chrome-host-bridge}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir/app/lib" "$output_dir/app/share/vulkan/icd.d"
"$repo_dir/tools/build-vortek.sh" "$output_dir/app/lib"
printf '%s\n' \
    '{' \
    '  "file_format_version": "1.0.0",' \
    '  "ICD": {' \
    '    "library_path": "libvulkan_vortek.so",' \
    '    "api_version": "1.3.128"' \
    '  }' \
    '}' > "$output_dir/app/share/vulkan/icd.d/vortek_icd.json"
"$repo_dir/tools/check-glibc-symbol-floor.py" "$output_dir/app" --maximum 2.41
printf 'chrome_package=external-arm64.tsv\nrootfs_payload=none\n' \
    > "$output_dir/BUILD-INFO"
printf '%s\n' "$output_dir"
