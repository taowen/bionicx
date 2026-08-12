#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="${1:?usage: tools/build-vortek.sh OUTPUT_LIB_DIR}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac

source_dir="$repo_dir/third_party/vortek"
if [[ ! -f "$source_dir/src/vulkan_calls.c" ]]; then
    echo "Vortek submodule is missing; run: git submodule update --init" >&2
    exit 2
fi

mkdir -p "$output_dir"
container_output="/work/${output_dir#"$repo_dir"/}"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" sh -eu -c '
        source_dir=/work/third_party/vortek
        output_dir="'"$container_output"'"
        # The generated serializer intentionally writes through Vulkan
        # nominally const graph while reconstructing wire objects. Keep its
        # upstream warning policy from obscuring actionable build failures.
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -w -pthread \
            -DVK_USE_PLATFORM_XLIB_KHR -DVK_USE_PLATFORM_XCB_KHR \
            -I"$source_dir/include" \
            "$source_dir/src/main.c" "$source_dir/src/vulkan_calls.c" \
            "$source_dir/src/vk_object.c" "$source_dir/src/vk_object_pool.c" \
            "$source_dir/src/arrays.c" \
            "$source_dir/src/descriptor_update_template.c" \
            "$source_dir/src/ring_buffer.c" \
            -Wl,-soname,libvulkan_vortek.so \
            -o "$output_dir/libvulkan_vortek.so" -ldl
    '

echo "$output_dir/libvulkan_vortek.so"
