#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/glx-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac

gladio_commit="116c0d14dedbea3bd057f98f1db138bb1efe225e"
gladio_sha256="d275a1e745d2d9388fe37061c379f5872820057a5b43f8bbc34d63c6c70c7024"
archive="$repo_dir/build/downloads/gladio-$gladio_commit.tar.gz"
source_dir="$output_dir/gladio-source"

"$repo_dir/examples/hello/build-bundle.sh" "$output_dir"
mkdir -p "$(dirname "$archive")" "$output_dir/app/lib"
if [[ ! -f "$archive" ]]; then
    curl -L --fail --show-error \
        "https://github.com/brunodev85/gladio/archive/$gladio_commit.tar.gz" \
        -o "$archive"
fi
echo "$gladio_sha256  $archive" | sha256sum --check --status
rm -rf "$source_dir"
mkdir -p "$source_dir"
tar -xzf "$archive" --strip-components=1 -C "$source_dir"
# The pinned archive mixes CRLF and LF files; normalize only the extracted
# build tree so the audited patch applies identically on every host.
sed -i 's/\r$//' "$source_dir/include/gl_context.h" \
    "$source_dir/include/gladio.h" "$source_dir/src/glx_calls.c" \
    "$source_dir/src/main.c"
patch -d "$source_dir" -p1 < \
    "$repo_dir/examples/glx-probe/gladio-dynamic-glx-opcode.patch"

# Gladio 1.0 hard-codes Winlator's private X11 path. Keep the protocol intact
# while targeting BionicX's equal-length Android package name.
sed -i 's|/data/data/com\.winlator/|/data/data/io.taowen.bx/|' \
    "$source_dir/include/gladio.h"

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
            -o "$app_dir/lib/libGL.so.1.7.0"
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
