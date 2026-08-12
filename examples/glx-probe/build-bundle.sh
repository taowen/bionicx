#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/glx-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac

"$repo_dir/examples/hello/build-bundle.sh" "$output_dir"
mkdir -p "$output_dir/app/lib"
"$repo_dir/tools/build-gladio.sh" "$output_dir/app/lib"
container_output="/work/${output_dir#"$repo_dir"/}"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" sh -eu -c '
        source_dir=/work/third_party/gladio
        app_dir="'"$container_output"'/app"
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            -I"$source_dir/include" examples/glx-probe/glx-probe.c \
            -L"$app_dir/lib" -Wl,-rpath,'"'"'$ORIGIN/../lib'"'"' \
            -o "$app_dir/bin/glx-probe" -lGL -lX11
    '

"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$output_dir/app/bin/glx-probe" \
    --search-root "$output_dir/app/lib" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --json "$output_dir/glx-probe-dependency-closure.json"
echo "$output_dir"
