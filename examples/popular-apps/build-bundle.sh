#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/popular-apps-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir/app/fixtures"
cp -a "$repo_dir/examples/popular-apps/fixtures/." \
    "$output_dir/app/fixtures/"
python3 "$repo_dir/examples/xfce-apps/build-png-fixtures.py" \
    "$output_dir/app/fixtures"
python3 "$repo_dir/examples/popular-apps/build-y4m-fixture.py" \
    "$output_dir/app/fixtures/bionicx-motion.y4m" \
    "$output_dir/app/fixtures/bionicx-motion.i420" \
    "$output_dir/app/fixtures/bionicx-tone.wav" \
    "$output_dir/app/fixtures/bionicx-motion-audio.avi"
"$repo_dir/tools/prepare-desktop-rootfs.sh" "$output_dir"

for required in \
        usr/bin/filezilla usr/bin/geany usr/bin/gimp usr/bin/inkscape \
        usr/lib/thunderbird/thunderbird usr/bin/vlc; do
    [[ -x "$output_dir/rootfs/$required" ]] || {
        echo "missing package-installed $required" >&2
        exit 1
    }
done
"$repo_dir/tools/check-glibc-symbol-floor.py" \
    "$output_dir/rootfs/usr/bin" --maximum 2.41
{
    for package in filezilla geany gimp inkscape thunderbird vlc; do
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
