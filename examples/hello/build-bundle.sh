#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/hello-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac
container_output="/work/${output_dir#"$repo_dir"/}"
mkdir -p "$output_dir/app/bin" "$output_dir/rootfs/usr/lib" "$repo_dir/build/cache"

builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" \
    aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/hello/hello-x11.c -o "$container_output/app/bin/hello-x11" -lX11

# Vanilla Debian glibc calls set_robust_list during startup and Android app
# seccomp kills it with SIGSYS. Use Winlator's pinned Android-compatible glibc
# runtime for the device bundle. Applications remain ordinary glibc ELFs.
rootfs_tzst="${BIONICX_ROOTFS_TZST:-}"
if [[ -z "$rootfs_tzst" ]]; then
    winlator_commit="c2f4ad4534f4637b543a9a3b085e28f50cf6d01c"
    archive="$repo_dir/build/cache/winlator-app-$winlator_commit.zip"
    rootfs_tzst="$repo_dir/build/cache/winlator-rootfs-$winlator_commit.tzst"
    if [[ ! -f "$archive" ]]; then
        curl -fL "https://github.com/brunodev85/winlator-app/archive/$winlator_commit.zip" \
            -o "$archive"
    fi
    if [[ ! -f "$rootfs_tzst" ]]; then
        unzip -p "$archive" "*/app/src/main/assets/rootfs.tzst" > "$rootfs_tzst"
    fi
fi
[[ -f "$rootfs_tzst" ]] || { echo "missing rootfs.tzst: $rootfs_tzst" >&2; exit 1; }

temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
tar --use-compress-program=unzstd -xf "$rootfs_tzst" -C "$temporary" ./usr/lib
for library in ld-linux-aarch64.so.1 libc.so.6 libm.so.6 libX11.so.6 libxcb.so.1 \
        libXau.so.6 libXdmcp.so.6; do
    cp -L "$temporary/usr/lib/$library" "$output_dir/rootfs/usr/lib/"
done
python3 "$repo_dir/tools/relocate-prefix.py" "$output_dir/rootfs" \
    --from-prefix /data/data/com.winlator --to-prefix /data/data/io.taowen.bx

"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$output_dir/app/bin/hello-x11" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --json "$output_dir/dependency-closure.json"
echo "$output_dir"
