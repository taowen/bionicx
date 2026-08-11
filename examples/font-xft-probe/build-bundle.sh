#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/font-xft-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac

"$repo_dir/examples/hello/build-bundle.sh" "$output_dir"
container_output="/work/${output_dir#"$repo_dir"/}"
builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" sh -eu -c '
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            -I/usr/include/freetype2 \
            examples/font-xft-probe/font-xft-probe.c \
            -o "'"$container_output"'/app/bin/font-xft-probe" \
            -lXft -lfontconfig -lfreetype -lXrender -lX11
        mkdir -p "'"$container_output"'/app/share/fonts"
        cp /usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf \
           /usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf \
           "'"$container_output"'/app/share/fonts"
        for library in libXft.so.2 libfontconfig.so.1 libfreetype.so.6 \
                       libexpat.so.1 libpng16.so.16 libz.so.1 libbz2.so.1.0 \
                       libbrotlidec.so.1 libbrotlicommon.so.1; do
            cp -L "/usr/lib/aarch64-linux-gnu/$library" \
                "'"$container_output"'/rootfs/usr/lib/$library"
        done
    '

"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$output_dir/app/bin/font-xft-probe" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --json "$output_dir/font-xft-probe-dependency-closure.json"
echo "$output_dir"
