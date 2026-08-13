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
cp "$repo_dir/examples/productivity-apps/fixtures/bionicx-artwork.svg" \
    "$output_dir/app/fixtures/"
cp "$repo_dir/examples/productivity-apps/fixtures/gimprc" \
    "$output_dir/app/fixtures/"
python3 "$repo_dir/examples/productivity-apps/build-image-fixture.py" \
    "$output_dir/app/fixtures/bionicx-image.ppm"
python3 "$repo_dir/examples/productivity-apps/build-odt-fixture.py" \
    "$output_dir/app/fixtures/bionicx-writer.odt"
python3 "$repo_dir/examples/wps/build-pdf-fixture.py" \
    "$output_dir/app/fixtures/bionicx-pages.pdf"
printf '%s\n' \
    'required_packages=firefox-esr libreoffice-writer evince gimp inkscape' \
    'rootfs_payload=none' > "$output_dir/BUILD-INFO"
printf '%s\n' "$output_dir"
