#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output="${1:-$repo_dir/build/audio-probe-bundle/app/bin/audio-probe}"
output="$(realpath -m "$output")"
case "$output" in
    "$repo_dir"/build/*) ;;
    *) echo "output must be below $repo_dir/build: $output" >&2; exit 2 ;;
esac

mkdir -p "$(dirname "$output")"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
container_output="/work/${output#"$repo_dir"/}"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/audio-probe/audio-probe.c -o "$container_output" \
        -lX11 -lpulse-simple -lpulse
echo "$output"
