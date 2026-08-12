#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/icewm-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

mkdir -p "$repo_dir/build/tmp"

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
"$repo_dir/tools/prepare-desktop-rootfs.sh" "$output_dir"
mkdir -p "$output_dir/app/bin" "$output_dir/app/share/icewm" \
    "$output_dir/app/etc/fonts"

builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work "$builder_image" sh -eu -c '
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            examples/icewm-probe/icewm-window.c \
            -o "'"${output_dir#"$repo_dir/"}"'/app/bin/icewm-window" -lX11
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            examples/icewm-probe/icewm-session.c \
            -o "'"${output_dir#"$repo_dir/"}"'/app/bin/icewm-session"
    '

desktop_rootfs="$repo_dir/build/desktop-rootfs-bundle/rootfs"
cp "$desktop_rootfs/usr/bin/icewm" "$output_dir/app/bin/"
cp -a "$desktop_rootfs/usr/share/icewm/." "$output_dir/app/share/icewm/"
cp "$repo_dir/examples/icewm-probe/preferences" \
    "$repo_dir/examples/icewm-probe/menu" "$output_dir/app/share/icewm/"
cp "$repo_dir/examples/icewm-probe/fonts.conf" \
    "$output_dir/app/etc/fonts/fonts.conf"
"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$output_dir/app/bin/icewm" \
    --entry "$output_dir/app/bin/icewm-window" \
    --entry "$output_dir/app/bin/icewm-session" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --json "$output_dir/icewm-dependency-closure.json"
"$repo_dir/tools/check-glibc-symbol-floor.py" "$output_dir/app" --maximum 2.41
echo "$output_dir"
