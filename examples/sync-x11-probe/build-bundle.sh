#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/sync-x11-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

mkdir -p "$output_dir/app/bin"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work "$builder_image" \
    aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/sync-x11-probe/sync-x11-probe.c \
        -o "${output_dir#"$repo_dir/"}/app/bin/sync-x11-probe" -lX11
interpreter=/data/user/0/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
rpath=/data/user/0/io.taowen.bx/files/rootfs/usr/lib:/data/user/0/io.taowen.bx/files/rootfs/usr/lib/aarch64-linux-gnu
patchelf --set-interpreter "$interpreter" --set-rpath "$rpath" \
    "$output_dir/app/bin/sync-x11-probe"
printf 'required_package=libx11-6\nrootfs_payload=none\n' \
    > "$output_dir/BUILD-INFO"
echo "$output_dir"
