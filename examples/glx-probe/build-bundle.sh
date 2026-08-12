#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/glx-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac

gladio_commit="90ac071f78b5ef9f4ba802d50c92bfcf068076ec"
gladio_sha256="70ae6ac71aca3d36099fcb0e7cda574095d12e3d500aad12b4c6502b6d5fa2ea"
archive="$repo_dir/build/downloads/gladio-$gladio_commit.tar.gz"
source_dir="$output_dir/gladio-source"

"$repo_dir/examples/hello/build-bundle.sh" "$output_dir"
mkdir -p "$(dirname "$archive")" "$output_dir/app/lib"
if [[ ! -f "$archive" ]]; then
    curl -L --fail --show-error \
        "https://github.com/taowen/gladio/archive/$gladio_commit.tar.gz" \
        -o "$archive"
fi
echo "$gladio_sha256  $archive" | sha256sum --check --status
rm -rf "$source_dir"
mkdir -p "$source_dir"
tar -xzf "$archive" --strip-components=1 -C "$source_dir"
container_output="/work/${output_dir#"$repo_dir"/}"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" sh -eu -c '
        source_dir="'"$container_output"'/gladio-source"
        app_dir="'"$container_output"'/app"
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -pthread \
            -DGL_GLEXT_PROTOTYPES -I"$source_dir/include" \
            "$source_dir/src/main.c" "$source_dir/src/gl_calls.c" \
            "$source_dir/src/glx_calls.c" "$source_dir/src/arrays.c" \
            "$source_dir/src/ring_buffer.c" "$source_dir/src/gl_buffer.c" \
            "$source_dir/src/gl_vao.c" -Wl,-soname,libGL.so.1 \
            -o "$app_dir/lib/libGL.so.1.7.0" -ldl
        ln -sf libGL.so.1.7.0 "$app_dir/lib/libGL.so.1"
        ln -sf libGL.so.1.7.0 "$app_dir/lib/libGL.so"
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
