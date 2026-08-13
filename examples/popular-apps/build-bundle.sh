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
python3 "$repo_dir/examples/popular-apps/build-torrent-fixture.py" \
    "$output_dir/app/fixtures/bionicx-network-payload.bin" \
    "$output_dir/app/fixtures/bionicx-webseed.torrent"
printf '%s\n' "$output_dir"
