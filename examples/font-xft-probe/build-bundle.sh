#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/font-xft-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

mkdir -p "$output_dir/app/bin" "$output_dir/app/share/fonts"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work "$builder_image" \
    sh -eu -c '
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            -I/usr/include/freetype2 \
            examples/font-xft-probe/font-xft-probe.c \
            -o "'"${output_dir#"$repo_dir/"}"'/app/bin/font-xft-probe" \
            -lXft -lfontconfig -lfreetype -lXrender -lX11
        cp /usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf \
           /usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf \
           "'"${output_dir#"$repo_dir/"}"'/app/share/fonts"
    '
interpreter=/data/user/0/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
rpath=/data/user/0/io.taowen.bx/files/rootfs/usr/lib:/data/user/0/io.taowen.bx/files/rootfs/usr/lib/aarch64-linux-gnu
patchelf --set-interpreter "$interpreter" --set-rpath "$rpath" \
    "$output_dir/app/bin/font-xft-probe"
printf 'required_package=libxft2\nrootfs_payload=none\n' \
    > "$output_dir/BUILD-INFO"
echo "$output_dir"
