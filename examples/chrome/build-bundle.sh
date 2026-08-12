#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/chrome-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

version=151.0.7922.108-1
chrome_sha256=23f5d27be6ad6f5d69c1c11b602d4ed25a8499cfdfa11c3ca479ad0b58285499

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir" "$repo_dir/build/tmp"
TMPDIR="$repo_dir/build/tmp" "$repo_dir/tools/prepare-desktop-rootfs.sh" "$output_dir"
desktop_bundle="$repo_dir/build/desktop-rootfs-bundle"
mkdir -p "$output_dir/app" "$output_dir/app/etc/fonts" \
    "$output_dir/app/lib" \
    "$output_dir/app/share/vulkan/icd.d"
cp -a "$desktop_bundle/apps/chrome/." "$output_dir/app/"
cp "$repo_dir/examples/chrome/fonts.conf" "$output_dir/app/etc/fonts/fonts.conf"

interpreter=/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
while IFS= read -r executable; do
    if patchelf --print-interpreter "$executable" >/dev/null 2>&1; then
        patchelf --set-interpreter "$interpreter" "$executable"
    fi
done < <(find "$output_dir/app/opt/google/chrome" -type f -perm /111 -print)

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

{
    printf 'chrome_version=%s\nchrome_sha256=%s\n' "$version" "$chrome_sha256"
    printf 'package_manifest_sha256=%s\n' \
        "$(sha256sum "$output_dir/packages.tsv" | cut -d' ' -f1)"
    printf 'desktop_rootfs_id=%s\n' \
        "$(<"$output_dir/rootfs/.bionicx-desktop-rootfs-id")"
    (cd "$output_dir" && find app/opt/google/chrome app/lib \
        -type f -exec sha256sum {} + | sort -k2)
} > "$output_dir/BUILD-INFO"
echo "$output_dir"
