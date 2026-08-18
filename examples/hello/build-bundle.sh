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

# Winlator supplies the audited Android path/syscall compatibility closure and
# X11 libraries. BionicX replaces its loader/libc/libm with a reproducible build
# of the Android-compatible glibc 2.41 recipe so owner-death robust mutexes retain their
# process-private semantics even though Android seccomp blocks set_robust_list.
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

# The Winlator archive contains a much larger usr/lib tree than the compact
# closure copied below. Do not consume the host's often size-limited /tmp
# while extracting it; all other reproducible build state already lives here.
mkdir -p "$repo_dir/build/tmp"
temporary="$(TMPDIR="$repo_dir/build/tmp" mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
tar --use-compress-program=unzstd -xf "$rootfs_tzst" -C "$temporary" ./usr/lib
for library in ld-linux-aarch64.so.1 libc.so.6 libm.so.6 libX11.so.6 libxcb.so.1 \
        libXau.so.6 libXdmcp.so.6 libXext.so.6 libXrender.so.1 \
        libXfixes.so.3 libXrandr.so.2 libXi.so.6 \
        libdl.so.2 libpthread.so.0 libresolv.so.2 librt.so.1 libutil.so.1; do
    cp -L "$temporary/usr/lib/$library" "$output_dir/rootfs/usr/lib/"
done
# WPS Spreadsheets loads libXtst through libetmain. Winlator's compact rootfs
# does not carry it, so take the reproducible ARM64 Debian library from the
# same content-addressed builder used for every glibc/X11 probe. Its required
# libc symbol floor is GLIBC_2.17 and is satisfied by our pinned glibc 2.41.
podman run --rm --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder_image" sh -c \
    "cp -L /usr/lib/aarch64-linux-gnu/libXtst.so.6 '$container_output/rootfs/usr/lib/libXtst.so.6'"
# A cold toolchain build emits upstream patch/build progress on stdout before
# its final artifact path. Keep the log on disk so the last line is still
# machine-readable when /dev/stderr is not a device (piped agent runs).
android_glibc_log="$repo_dir/build/cache/android-glibc-last-build.log"
"$repo_dir/tools/build-android-glibc.sh" | tee "$android_glibc_log"
android_glibc="$(tail -n 1 "$android_glibc_log")"
for library in ld-linux-aarch64.so.1 libc.so.6 libm.so.6; do
    cp "$android_glibc/$library" "$output_dir/rootfs/usr/lib/"
done
mkdir -p "$output_dir/rootfs/usr/lib/locale"
cp -a "$temporary/usr/lib/locale/en_US.utf8" \
    "$output_dir/rootfs/usr/lib/locale/"
python3 "$repo_dir/tools/relocate-prefix.py" "$output_dir/rootfs" \
    --from-prefix /data/data/com.winlator --to-prefix /data/data/io.taowen.bx

"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$output_dir/app/bin/hello-x11" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --json "$output_dir/dependency-closure.json"
echo "$output_dir"
