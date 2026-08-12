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
"$repo_dir/tools/prepare-desktop-rootfs.sh" "$output_dir"
for required in usr/bin/dbus-daemon usr/bin/dbus-send; do
    [[ -x "$output_dir/rootfs/$required" ]] || {
        echo "missing package-installed $required" >&2
        exit 1
    }
done
{
    printf 'dbus_package=%s\n' \
        "$(awk -F '\t' '$1 == "dbus" || $1 == "dbus:arm64" {print $2}' \
            "$output_dir/packages.tsv")"
    printf 'desktop_rootfs_id=%s\n' \
        "$(<"$output_dir/rootfs/.bionicx-desktop-rootfs-id")"
} > "$output_dir/BUILD-INFO"
printf '%s\n' "$output_dir"
