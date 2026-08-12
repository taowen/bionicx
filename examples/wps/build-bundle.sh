#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/wps-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir" "$repo_dir/build/tmp"
TMPDIR="$repo_dir/build/tmp" "$repo_dir/tools/prepare-desktop-rootfs.sh" "$output_dir"

desktop_bundle="$repo_dir/build/desktop-rootfs-bundle"
mkdir -p "$output_dir/app" "$output_dir/app/etc/fonts" "$output_dir/app/bin"
cp -a "$desktop_bundle/apps/wps-office/." "$output_dir/app/"
cp "$repo_dir/examples/wps/fonts.conf" "$output_dir/app/etc/fonts/fonts.conf"

office="$output_dir/app/opt/kingsoft/wps-office/office6"
interpreter=/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
# WPS's postinst selects a newer system libstdc++ by replacing its private
# symlink with an absolute FHS path.  There is no chroot/proot on Android;
# remove that package-generated link so the shared Debian multiarch directory
# selected by LD_LIBRARY_PATH provides the same system library.
libstdcpp_target="$(readlink "$office/libstdc++.so.6" || true)"
if [[ "$libstdcpp_target" == /usr/lib/* ]]; then
    rm "$office/libstdc++.so.6"
fi
for entrypoint in wps et wpp wpspdf; do
    [[ -f "$office/$entrypoint" ]] || {
        echo "missing WPS entrypoint: $entrypoint" >&2
        exit 1
    }
    patchelf --set-interpreter "$interpreter" "$office/$entrypoint"
done
# WPS Presentation creates a relative ??/PreXXXXXXXX template under LANG=C.
mkdir -p "$office/??"
chmod 0700 "$office/??"

builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work "$builder_image" \
    aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        native/desktop/bionicx-open.c \
        -o "${output_dir#"$repo_dir/"}/app/bin/xdg-open"
patchelf --set-interpreter "$interpreter" "$output_dir/app/bin/xdg-open"

"$repo_dir/tools/check-glibc-symbol-floor.py" "$output_dir/app" --maximum 2.41
{
    printf 'wps_version=11.1.0.11720\n'
    printf 'wps_sha256=172d8bcf3e3bc756994ae5adf66f63f22691e1ab0d18986c50bb6b6ab7f62948\n'
    printf 'package_manifest_sha256=%s\n' \
        "$(sha256sum "$output_dir/packages.tsv" | cut -d' ' -f1)"
    printf 'desktop_rootfs_id=%s\n' \
        "$(<"$output_dir/rootfs/.bionicx-desktop-rootfs-id")"
} > "$output_dir/BUILD-INFO"
printf '%s\n' "$output_dir"
