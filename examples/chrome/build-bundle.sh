#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/chrome-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

version=151.0.7922.108-1
deb_name="google-chrome-stable_${version}_arm64.deb"
chrome_sha256=23f5d27be6ad6f5d69c1c11b602d4ed25a8499cfdfa11c3ca479ad0b58285499
chrome_url="https://dl.google.com/linux/chrome/deb/pool/main/g/google-chrome-stable/$deb_name"
cache_dir="$repo_dir/build/cache/chrome-$version"
deb_dir="$cache_dir/debs"
lock_file="$repo_dir/examples/chrome/dependencies.lock"
mkdir -p "$deb_dir" "$repo_dir/build/tmp"

chrome_deb="$deb_dir/$deb_name"
if [[ ! -f "$chrome_deb" ]]; then
    temporary="$chrome_deb.download"
    curl -fL "$chrome_url" -o "$temporary"
    echo "$chrome_sha256  $temporary" | sha256sum -c -
    mv "$temporary" "$chrome_deb"
fi
echo "$chrome_sha256  $chrome_deb" | sha256sum -c -

lock_matches() {
    (cd "$deb_dir" && sha256sum -c "$lock_file" >/dev/null) || return 1
    diff -u \
        <(awk '{print $2}' "$lock_file" | sort) \
        <(find "$deb_dir" -maxdepth 1 -type f -name '*.deb' -printf '%f\n' | sort) \
        >/dev/null
}

if ! lock_matches; then
    find "$deb_dir" -maxdepth 1 -type f -name '*.deb' ! -name "$deb_name" -delete
    # Resolve from a native ARM64 userspace. An amd64 apt host may consider its
    # own installed libraries sufficient and silently omit the ARM64 closure.
    podman run --rm --arch arm64 --network host \
        --env CHROME_VERSION="$version" --env CHROME_DEB_NAME="$deb_name" \
        --volume "$repo_dir:/work:Z" --workdir /work \
        docker.io/library/debian:trixie-slim sh -eu -c '
            deb_dir="/work/build/cache/chrome-$CHROME_VERSION/debs"
            apt-get update >/dev/null
            apt-get install -y --no-install-recommends --download-only \
                "$deb_dir/$CHROME_DEB_NAME" >/dev/null
            cp /var/cache/apt/archives/*.deb "$deb_dir/"
            cd "$deb_dir"
            # These libraries are already installed in debian:trixie-slim, so
            # apt would otherwise omit them from a portable extracted closure.
            apt-get download \
                libblkid1 libbz2-1.0 libcap2 libgcc-s1 libgmp10 \
                libhogweed6t64 libmount1 \
                liblzma5 libnettle8t64 libpcre2-8-0 libselinux1 \
                libsqlite3-0 libstdc++6 libsystemd0 libudev1 libzstd1 \
                zlib1g >/dev/null
        '
    if ! lock_matches; then
        echo "Chrome dependency set drifted from examples/chrome/dependencies.lock" >&2
        echo "refresh and review the lock deliberately before accepting new packages" >&2
        exit 1
    fi
fi

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir"
TMPDIR="$repo_dir/build/tmp" "$repo_dir/examples/hello/build-bundle.sh" "$output_dir"
rm -f "$output_dir/app/bin/hello-x11"

temporary="$(mktemp -d "$repo_dir/build/chrome-stage.XXXXXXXX")"
cleanup() {
    case "$temporary" in
        "$repo_dir/build/chrome-stage."*) rm -rf -- "$temporary" ;;
        *) echo "refusing to clean unexpected path: $temporary" >&2 ;;
    esac
}
trap cleanup EXIT
mkdir -p "$temporary/extracted"

builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --userns=keep-id \
    --env CHROME_VERSION="$version" \
    --env STAGE_REL="${temporary#"$repo_dir/"}" \
    --volume "$repo_dir:/work:Z" --workdir /work "$builder_image" sh -eu -c '
        for package in build/cache/chrome-$CHROME_VERSION/debs/*.deb; do
            dpkg-deb -x "$package" "$STAGE_REL/extracted"
        done
    '

mkdir -p "$output_dir/app/opt/google" "$output_dir/app/etc/fonts" \
    "$output_dir/app/lib" "$output_dir/app/share/glib-2.0" \
    "$output_dir/app/share/mime" "$output_dir/app/share/icons"
cp -a "$temporary/extracted/opt/google/chrome" "$output_dir/app/opt/google/"
cp "$repo_dir/examples/chrome/fonts.conf" "$output_dir/app/etc/fonts/fonts.conf"
cp -a "$temporary/extracted/usr/share/glib-2.0/schemas" \
    "$output_dir/app/share/glib-2.0/"
glib-compile-schemas "$output_dir/app/share/glib-2.0/schemas"
cp -a "$temporary/extracted/usr/share/mime/packages" \
    "$output_dir/app/share/mime/"
update-mime-database "$output_dir/app/share/mime"
for theme in Adwaita hicolor; do
    cp -a "$temporary/extracted/usr/share/icons/$theme" \
        "$output_dir/app/share/icons/"
    gtk-update-icon-cache --force "$output_dir/app/share/icons/$theme"
done

interpreter=/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
while IFS= read -r executable; do
    if patchelf --print-interpreter "$executable" >/dev/null 2>&1; then
        patchelf --set-interpreter "$interpreter" "$executable"
    fi
done < <(find "$output_dir/app/opt/google/chrome" -type f -perm /111 -print)

library_root="$temporary/extracted/usr/lib/aarch64-linux-gnu"
pixbuf_loader_root="$library_root/gdk-pixbuf-2.0/2.10.0/loaders"
runtime_modules=(
    # Chromium loads the native Linux UI with dlopen(), so GTK is not visible
    # from the chrome executable's DT_NEEDED graph.
    libgtk-3.so.0
    libsoftokn3.so
    libfreebl3.so
    libfreeblpriv3.so
    libnssckbi.so
    libnssdbm3.so
)
entries=(
    --entry "$output_dir/app/opt/google/chrome/chrome"
    --entry "$output_dir/app/opt/google/chrome/chrome_crashpad_handler"
    --entry "$output_dir/app/opt/google/chrome/chrome-management-service"
)
for module in "${runtime_modules[@]}"; do
    [[ -f "$library_root/$module" ]] || {
        echo "missing declared Chrome runtime module: $module" >&2
        exit 1
    }
    entries+=(--entry "$library_root/$module")
done
for loader in "$pixbuf_loader_root"/*.so; do
    [[ -f "$loader" ]] || {
        echo "missing GDK Pixbuf loader modules" >&2
        exit 1
    }
    entries+=(--entry "$loader")
done

"$repo_dir/tools/resolve-elf-deps.py" \
    "${entries[@]}" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --search-root "$output_dir/app/opt/google/chrome" \
    --search-root "$library_root" \
    --exclude-copy-root "$output_dir/app" \
    --copy-to "$output_dir/app/lib" \
    --json "$output_dir/chrome-dependency-closure.json"
for checksum in "$library_root"/*.chk; do
    cp "$checksum" "$output_dir/app/lib/"
done
"$repo_dir/tools/build-gladio.sh" "$output_dir/app/lib"

# The ARM64 query tool must inspect the ARM64 plugins themselves. Run it in a
# native-architecture build container, then rewrite its build-stage paths to
# the fixed app-private Android install location.
pixbuf_cache_dir="$output_dir/app/lib/gdk-pixbuf-2.0/2.10.0"
mkdir -p "$pixbuf_cache_dir"
output_rel="${output_dir#"$repo_dir/"}"
stage_rel="${temporary#"$repo_dir/"}"
podman run --rm --arch arm64 --network none --userns=keep-id \
    --env OUTPUT_REL="$output_rel" --env STAGE_REL="$stage_rel" \
    --volume "$repo_dir:/work:Z" --workdir /work \
    docker.io/library/debian:trixie-slim sh -eu -c '
        library_root="/work/$STAGE_REL/extracted/usr/lib/aarch64-linux-gnu"
        LD_LIBRARY_PATH="$library_root" \
            "$library_root/gdk-pixbuf-2.0/gdk-pixbuf-query-loaders" \
            "$library_root"/gdk-pixbuf-2.0/2.10.0/loaders/*.so \
            > "/work/$OUTPUT_REL/app/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache.stage"
    '
container_loader_root="/work/$stage_rel/extracted/usr/lib/aarch64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders"
device_app_root=/data/data/io.taowen.bx/files/apps/chrome
sed "s|$container_loader_root/|$device_app_root/lib/|g" \
    "$pixbuf_cache_dir/loaders.cache.stage" > "$pixbuf_cache_dir/loaders.cache"
rm "$pixbuf_cache_dir/loaders.cache.stage"

{
    printf 'chrome_version=%s\nchrome_sha256=%s\n' "$version" "$chrome_sha256"
    printf 'dependency_lock_sha256='
    sha256sum "$lock_file" | cut -d' ' -f1
    printf 'runtime_modules=%s\n' "${runtime_modules[*]}"
    (cd "$output_dir" && find app/opt/google/chrome app/lib \
        app/share/glib-2.0/schemas app/share/mime app/share/icons \
        -type f -exec sha256sum {} + | sort -k2)
} > "$output_dir/BUILD-INFO"
echo "$output_dir"
