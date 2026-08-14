#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/vulkan-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac

"$repo_dir/examples/hello/build-bundle.sh" "$output_dir"
mkdir -p "$output_dir/app/lib" "$output_dir/app/share/vulkan/icd.d" \
    "$output_dir/app/share/vulkan-probe"
"$repo_dir/tools/build-vortek.sh" "$output_dir/app/lib"
glslangValidator -V -S vert \
    "$repo_dir/examples/vulkan-probe/triangle.vert" \
    -o "$output_dir/app/share/vulkan-probe/triangle.vert.spv" >/dev/null
glslangValidator -V -S frag \
    "$repo_dir/examples/vulkan-probe/triangle.frag" \
    -o "$output_dir/app/share/vulkan-probe/triangle.frag.spv" >/dev/null

container_output="/work/${output_dir#"$repo_dir"/}"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" sh -eu -c '
        app_dir="'"$container_output"'/app"
        compile() {
            aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
                -DVK_USE_PLATFORM_XLIB_KHR -DVK_USE_PLATFORM_XCB_KHR \
                examples/vulkan-probe/common.c \
                "examples/vulkan-probe/$1.c" \
                -Wl,-rpath,'"'"'$ORIGIN/../lib'"'"' \
                -o "$app_dir/bin/$1" \
                -lvulkan -lX11-xcb -lX11 -lxcb $2
        }
        compile vulkan-wsi ""
        compile vulkan-present -lpthread
        compile vulkan-lifetime ""
        cp -L /usr/lib/aarch64-linux-gnu/libvulkan.so.1 "$app_dir/lib/"
        cp -L /usr/lib/aarch64-linux-gnu/libX11-xcb.so.1 "$app_dir/lib/"
    '

cat > "$output_dir/app/share/vulkan/icd.d/vortek_icd.json" <<'EOF'
{
  "file_format_version": "1.0.0",
  "ICD": {
    "library_path": "../../../lib/libvulkan_vortek.so",
    "api_version": "1.3.128"
  }
}
EOF

"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$output_dir/app/bin/vulkan-present" \
    --search-root "$output_dir/app/lib" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --json "$output_dir/vulkan-probe-dependency-closure.json"
echo "$output_dir"
