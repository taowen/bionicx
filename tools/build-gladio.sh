#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="${1:?usage: tools/build-gladio.sh OUTPUT_LIB_DIR}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac

source_dir="$repo_dir/third_party/gladio"
if [[ ! -f "$source_dir/src/main.c" ]]; then
    echo "Gladio submodule is missing; run: git submodule update --init" >&2
    exit 2
fi

mkdir -p "$output_dir"
container_output="/work/${output_dir#"$repo_dir"/}"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" sh -eu -c '
        source_dir=/work/third_party/gladio
        output_dir="'"$container_output"'"
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -pthread \
            -DGL_GLEXT_PROTOTYPES -I"$source_dir/include" \
            "$source_dir/src/main.c" "$source_dir/src/gl_calls.c" \
            "$source_dir/src/glx_calls.c" "$source_dir/src/arrays.c" \
            "$source_dir/src/ring_buffer.c" "$source_dir/src/gl_buffer.c" \
            "$source_dir/src/gl_vao.c" -Wl,-soname,libGL.so.1 \
            -o "$output_dir/libGL.so.1.7.0" -ldl
        ln -sf libGL.so.1.7.0 "$output_dir/libGL.so.1"
        ln -sf libGL.so.1.7.0 "$output_dir/libGL.so"
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -pthread \
            -DGL_GLEXT_PROTOTYPES -I"$source_dir/include" \
            "$source_dir/src/egl.c" \
            -L"$output_dir" -lGL -Wl,-soname,libEGL.so.1 \
            -o "$output_dir/libEGL.so.1.0.0"
        ln -sf libEGL.so.1.0.0 "$output_dir/libEGL.so.1"
        ln -sf libEGL.so.1.0.0 "$output_dir/libEGL.so"
    '

echo "$output_dir/libGL.so.1.7.0"
