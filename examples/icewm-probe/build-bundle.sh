#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/icewm-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

cache_dir="$repo_dir/build/cache/icewm-3.7.4"
deb_dir="$cache_dir/debs"
lock_file="$repo_dir/examples/icewm-probe/dependencies.lock"
mkdir -p "$deb_dir" "$repo_dir/build/tmp"

lock_matches() {
    (cd "$deb_dir" && sha256sum -c "$lock_file" >/dev/null) || return 1
    diff -u <(awk '{print $2}' "$lock_file" | sort) \
        <(find "$deb_dir" -maxdepth 1 -type f -name '*.deb' -printf '%f\n' | sort) \
        >/dev/null
}

if ! lock_matches; then
    find "$deb_dir" -mindepth 1 -maxdepth 1 -type f -delete
    podman run --rm --arch arm64 --network host \
        --volume "$repo_dir:/work:Z" --workdir /work \
        docker.io/library/debian:trixie-slim sh -eu -c '
            apt-get update >/dev/null
            cd build/cache/icewm-3.7.4/debs
            apt-get download icewm icewm-common libsm6 libice6 libfribidi0 \
                libxft2 libxinerama1 libxcomposite1 libxdamage1 libimlib2t64 \
                libstdc++6 libgcc-s1 libxcb-shm0 libx11-xcb1 libfreetype6 \
                libuuid1 libbrotli1 libbz2-1.0 libfontconfig1 \
                libpng16-16t64 zlib1g libexpat1 >/dev/null
        '
    lock_matches || {
        echo "IceWM dependency set drifted from dependencies.lock" >&2
        exit 1
    }
fi

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
"$repo_dir/examples/hello/build-bundle.sh" "$output_dir"
stage="$(mktemp -d "$repo_dir/build/icewm-stage.XXXXXXXX")"
trap 'find "$stage" -mindepth 1 -delete; rmdir "$stage"' EXIT
mkdir -p "$stage/extracted" "$output_dir/app/lib/imlib2/loaders" \
    "$output_dir/app/share/icewm" "$output_dir/app/etc/fonts"

builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work "$builder_image" sh -eu -c '
        for package in build/cache/icewm-3.7.4/debs/*.deb; do
            dpkg-deb -x "$package" "'"${stage#"$repo_dir/"}"'/extracted"
        done
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            examples/icewm-probe/icewm-window.c \
            -o "'"${output_dir#"$repo_dir/"}"'/app/bin/icewm-window" -lX11
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            examples/icewm-probe/icewm-session.c \
            -o "'"${output_dir#"$repo_dir/"}"'/app/bin/icewm-session"
    '

cp "$stage/extracted/usr/bin/icewm" "$output_dir/app/bin/"
cp -a "$stage/extracted/usr/share/icewm/." "$output_dir/app/share/icewm/"
cp "$repo_dir/examples/icewm-probe/preferences" \
    "$repo_dir/examples/icewm-probe/menu" "$output_dir/app/share/icewm/"
cp "$repo_dir/examples/icewm-probe/fonts.conf" \
    "$output_dir/app/etc/fonts/fonts.conf"
cp "$stage/extracted/usr/lib/aarch64-linux-gnu/imlib2/loaders/xpm.so" \
    "$output_dir/app/lib/imlib2/loaders/"

library_root="$stage/extracted/usr/lib/aarch64-linux-gnu"
"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$output_dir/app/bin/icewm" \
    --entry "$output_dir/app/bin/icewm-window" \
    --entry "$output_dir/app/bin/icewm-session" \
    --entry "$output_dir/app/lib/imlib2/loaders/xpm.so" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --search-root "$library_root" \
    --copy-to "$output_dir/app/lib" \
    --exclude-copy-root "$output_dir/rootfs/usr/lib" \
    --json "$output_dir/icewm-dependency-closure.json"
echo "$output_dir"
