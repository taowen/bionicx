#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/x11-desktop-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac

"$repo_dir/examples/hello/build-bundle.sh" "$output_dir"
container_output="/work/${output_dir#"$repo_dir"/}"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" \
    aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/x11-desktop-probe/x11-desktop-probe.c \
        -o "$container_output/app/bin/x11-desktop-probe" \
        -lXrender -lXfixes -lXrandr -lXi -lXext -lX11

"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$output_dir/app/bin/x11-desktop-probe" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --json "$output_dir/x11-desktop-probe-dependency-closure.json"
echo "$output_dir"
