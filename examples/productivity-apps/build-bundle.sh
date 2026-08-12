#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/productivity-apps-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir/app/fixtures"
cp "$repo_dir/examples/productivity-apps/fixtures/bionicx-page.html" \
    "$output_dir/app/fixtures/"
python3 "$repo_dir/examples/productivity-apps/build-odt-fixture.py" \
    "$output_dir/app/fixtures/bionicx-writer.odt"
python3 "$repo_dir/examples/wps/build-pdf-fixture.py" \
    "$output_dir/app/fixtures/bionicx-pages.pdf"
"$repo_dir/tools/prepare-desktop-rootfs.sh" "$output_dir"

for required in \
        usr/lib/firefox-esr/firefox-esr \
        usr/lib/libreoffice/program/soffice.bin \
        usr/bin/evince; do
    [[ -x "$output_dir/rootfs/$required" ]] || {
        echo "missing package-installed $required" >&2
        exit 1
    }
done
"$repo_dir/tools/check-glibc-symbol-floor.py" \
    "$output_dir/rootfs/usr/bin" --maximum 2.41
{
    for package in firefox-esr libreoffice-writer evince; do
        printf '%s_package=%s\n' "${package//-/_}" \
            "$(awk -F '\t' -v name="$package" \
                '$1 == name || $1 == name ":arm64" {print $2}' \
                "$output_dir/packages.tsv")"
    done
    printf 'desktop_rootfs_id=%s\n' \
        "$(<"$output_dir/rootfs/.bionicx-desktop-rootfs-id")"
} > "$output_dir/BUILD-INFO"
printf '%s\n' "$output_dir"
