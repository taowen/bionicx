#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output="${1:-$repo_dir/build/gtk3-probe}"
output="$(realpath -m "$output")"
case "$output" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output" >&2; exit 2 ;;
esac

mkdir -p "$(dirname "$output")"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
container_output="/work/${output#"$repo_dir"/}"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/gtk3-probe/gtk3-probe.c -o "$container_output" -ldl
patchelf --set-interpreter \
    /data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1 \
    "$output"
echo "$output"
