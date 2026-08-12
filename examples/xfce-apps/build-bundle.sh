#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/xfce-apps-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir/app/fixtures"
cp -a "$repo_dir/examples/xfce-apps/fixtures/." \
    "$output_dir/app/fixtures/"
python3 "$repo_dir/examples/xfce-apps/build-png-fixtures.py" \
    "$output_dir/app/fixtures"
"$repo_dir/tools/prepare-desktop-rootfs.sh" "$output_dir"

for required in usr/bin/thunar usr/bin/mousepad usr/bin/ristretto; do
    [[ -x "$output_dir/rootfs/$required" ]] || {
        echo "missing package-installed $required" >&2
        exit 1
    }
done
"$repo_dir/tools/check-glibc-symbol-floor.py" \
    "$output_dir/rootfs/usr/bin" --maximum 2.41
{
    for package in thunar mousepad ristretto; do
        printf '%s_package=%s\n' "$package" \
            "$(awk -F '\t' -v name="$package" \
                '$1 == name || $1 == name ":arm64" {print $2}' \
                "$output_dir/packages.tsv")"
    done
    printf 'package_manifest_sha256=%s\n' \
        "$(sha256sum "$output_dir/packages.tsv" | cut -d' ' -f1)"
    printf 'desktop_rootfs_id=%s\n' \
        "$(<"$output_dir/rootfs/.bionicx-desktop-rootfs-id")"
} > "$output_dir/BUILD-INFO"
printf '%s\n' "$output_dir"
