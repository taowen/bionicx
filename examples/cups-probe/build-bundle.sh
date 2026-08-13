#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/cups-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir/app/bin"

cache="$repo_dir/build/cache"
mkdir -p "$cache/cups-probe-dev"
dev_deb="libcups2-dev_2.4.10-3+deb13u2_arm64.deb"
lib_deb="libcups2t64_2.4.10-3+deb13u2_arm64.deb"
base="http://snapshot.debian.org/archive/debian/20260811T000000Z/pool/main/c/cups"
extract_deb() {
    local archive="$1"
    local dest="$2"
    local member
    member="$(ar t "$archive" | awk '/^data\.tar/{ print; exit }')"
    [[ -n "$member" ]] || { echo "missing data.tar in $archive" >&2; return 1; }
    case "$member" in
        *.xz) ar p "$archive" "$member" | xz -dc | tar -x -C "$dest" ;;
        *.zst) ar p "$archive" "$member" | zstd -dc | tar -x -C "$dest" ;;
        *.gz) ar p "$archive" "$member" | gzip -dc | tar -x -C "$dest" ;;
        *) echo "unsupported data archive: $member" >&2; return 1 ;;
    esac
}
for deb in "$dev_deb" "$lib_deb"; do
    if [[ ! -f "$cache/$deb" ]]; then
        curl -fL "$base/$deb" -o "$cache/$deb"
    fi
    extract_deb "$cache/$deb" "$cache/cups-probe-dev"
done

builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work "$builder_image" \
    aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        -I build/cache/cups-probe-dev/usr/include \
        -L build/cache/cups-probe-dev/usr/lib/aarch64-linux-gnu \
        examples/cups-probe/cups-probe.c \
        -Wl,--allow-shlib-undefined \
        -o "${output_dir#"$repo_dir/"}/app/bin/cups-probe" -lcups
interpreter=/data/user/0/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
rpath=/data/user/0/io.taowen.bx/files/rootfs/usr/lib:/data/user/0/io.taowen.bx/files/rootfs/usr/lib/aarch64-linux-gnu
patchelf --set-interpreter "$interpreter" --set-rpath "$rpath" \
    "$output_dir/app/bin/cups-probe"
printf 'required_package=cups-client\nrootfs_payload=none\n' \
    > "$output_dir/BUILD-INFO"
echo "$output_dir"
