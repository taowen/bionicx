#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="${1:?usage: tools/build-desktop-rootfs.sh OUTPUT_BUNDLE_DIR}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

# Debian 13 (trixie) is BionicX's userspace ABI/package baseline.  Both the
# OCI base and archive timestamp are immutable so a future apt repository
# update cannot silently change an existing rootfs build.
debian_snapshot=20260811T000000Z
base_image="docker.io/library/debian@sha256:c94f5ddd41327aa2d4a7cfba7889056c02936182fd76a513fec6160c97181fc0"
chrome_version=151.0.7922.108-1
chrome_name="google-chrome-stable_${chrome_version}_arm64.deb"
chrome_sha256=23f5d27be6ad6f5d69c1c11b602d4ed25a8499cfdfa11c3ca479ad0b58285499
chrome_url="https://dl.google.com/linux/chrome/deb/pool/main/g/google-chrome-stable/$chrome_name"
wps_name=wps-office_11.1.0.11720_arm64.deb
wps_sha256=172d8bcf3e3bc756994ae5adf66f63f22691e1ab0d18986c50bb6b6ab7f62948
wps_url="https://github.com/Pi-Apps-Coders/files/releases/download/large-files/$wps_name"
chrome_deb="$repo_dir/build/cache/chrome-$chrome_version/debs/$chrome_name"
wps_deb="$repo_dir/build/cache/wps-11.1.0.11720/$wps_name"

fetch_checked() {
    local url="$1" checksum="$2" destination="$3"
    mkdir -p "$(dirname "$destination")"
    if [[ ! -f "$destination" ]]; then
        curl -fL "$url" -o "$destination.download"
        printf '%s  %s\n' "$checksum" "$destination.download" | sha256sum -c -
        mv "$destination.download" "$destination"
    fi
    printf '%s  %s\n' "$checksum" "$destination" | sha256sum -c - >/dev/null
}
fetch_checked "$chrome_url" "$chrome_sha256" "$chrome_deb"
fetch_checked "$wps_url" "$wps_sha256" "$wps_deb"

install_script="$repo_dir/tools/install-trixie-desktop-rootfs.sh"
input_id="$({
    printf '%s\n%s\n%s\n%s\n' \
        "$base_image" "$debian_snapshot" "$chrome_sha256" "$wps_sha256"
    # Hash content only: cache identity must not depend on the clone's absolute
    # host path (sha256sum normally includes the filename in its output).
    sha256sum "$install_script" | cut -d' ' -f1
} | sha256sum | cut -d' ' -f1)"
image="localhost/bionicx-trixie-desktop:$input_id"
container="bionicx-trixie-desktop-${input_id:0:12}-$$"
temporary="$(mktemp -d "$repo_dir/build/trixie-rootfs.XXXXXXXX")"
apt_cache="$repo_dir/build/cache/trixie-desktop-apt/archives"
mkdir -p "$apt_cache"
cleanup() {
    podman rm -f "$container" >/dev/null 2>&1 || true
    case "$temporary" in
        "$repo_dir/build/trixie-rootfs."*) rm -rf -- "$temporary" ;;
        *) echo "refusing to clean unexpected path: $temporary" >&2 ;;
    esac
}
trap cleanup EXIT

if ! podman image exists "$image"; then
    podman create --name "$container" --arch arm64 --network host \
        --env BIONICX_DEBIAN_SNAPSHOT="$debian_snapshot" \
        "$base_image" /bin/sh /tmp/install-bionicx-desktop.sh >/dev/null
    podman cp "$install_script" "$container:/tmp/install-bionicx-desktop.sh"
    podman cp "$chrome_deb" "$container:/tmp/google-chrome.deb"
    podman cp "$wps_deb" "$container:/tmp/wps-office.deb"
    # Seed apt's container-owned cache without bind-mounting host ownership or
    # SELinux labels into dpkg maintainer-script execution.
    if compgen -G "$apt_cache/*.deb" >/dev/null; then
        (cd "$apt_cache" && find . -maxdepth 1 -type f -name '*.deb' -print0 | \
            tar --null -T - -cf -) | \
            podman cp - "$container:/tmp/"
    fi
    podman start --attach "$container"
    podman commit "$container" "$image" >/dev/null
    podman rm "$container" >/dev/null
fi

mkdir -p "$temporary/exported"
podman create --name "$container" "$image" /bin/true >/dev/null
podman export "$container" | tar --no-same-owner -xf - -C "$temporary/exported"
podman rm "$container" >/dev/null

mkdir -p "$output_dir"
# Rootless Podman exports container-root files with subordinate host IDs.  Do
# cleanup and ownership normalization in the same user namespace so subsequent
# deterministic post-processing never depends on host root privileges.
podman unshare find "$output_dir" -mindepth 1 -delete
mkdir -p "$output_dir/rootfs" "$output_dir/apps"
cp -a "$temporary/exported/etc" "$temporary/exported/usr" \
    "$temporary/exported/var" "$output_dir/rootfs/"
for merged_path in bin lib lib64 sbin; do
    [[ ! -e "$temporary/exported/$merged_path" ]] || \
        cp -a "$temporary/exported/$merged_path" "$output_dir/rootfs/"
done
cp -a "$temporary/exported/bionicx/apps/." "$output_dir/apps/"
cp -a "$temporary/exported/bionicx/metadata/." "$output_dir/"
podman unshare chown -R 0:0 "$output_dir"
chmod u+w "$output_dir/rootfs/etc/machine-id"
printf '%s' '21eeeb73de5941e9a77d41bb0b9953d5' \
    > "$output_dir/rootfs/etc/machine-id"

# Overlay the Android-seccomp/path-compatible loader and libc at the first
# library-search location.  Debian's original multiarch files remain intact as
# package-owned data; BionicX processes always enter through this loader.
compat="$temporary/compat"
"$repo_dir/examples/hello/build-bundle.sh" "$compat"
rm -f "$compat/app/bin/hello-x11"
cp -a "$compat/rootfs/usr/lib/." "$output_dir/rootfs/usr/lib/"
"$repo_dir/tools/build-gladio.sh" "$output_dir/rootfs/usr/lib"

# Debian's generated pixbuf cache names modules by absolute FHS paths.  There
# is intentionally no chroot/proot on Android, so relocate those text entries
# to the app-private runtime prefix while retaining Debian's directory layout.
pixbuf_cache="$output_dir/rootfs/usr/lib/aarch64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders.cache"
if [[ -f "$pixbuf_cache" ]]; then
    device_root=/data/data/io.taowen.bx/files/rootfs
    sed -i "s|\"/usr/lib/|\"$device_root/usr/lib/|g" "$pixbuf_cache"
fi

"$repo_dir/tools/check-glibc-symbol-floor.py" "$output_dir/rootfs" --maximum 2.41

manifest="$output_dir/rootfs/.bionicx-desktop-rootfs.manifest"
{
    printf 'distribution=debian-13-trixie\n'
    printf 'debian_snapshot=%s\n' "$debian_snapshot"
    printf 'base_image=%s\n' "$base_image"
    printf 'chrome_sha256=%s\n' "$chrome_sha256"
    printf 'wps_sha256=%s\n' "$wps_sha256"
    printf 'package_manifest_sha256=%s\n' "$(sha256sum "$output_dir/packages.tsv" | cut -d' ' -f1)"
    (cd "$output_dir/rootfs" && find . -type f \
        ! -name '.bionicx-desktop-rootfs.manifest' \
        ! -name '.bionicx-desktop-rootfs-id' -exec sha256sum {} + | sort -k2)
} > "$manifest"
sha256sum "$manifest" | cut -d' ' -f1 \
    > "$output_dir/rootfs/.bionicx-desktop-rootfs-id"
printf '%s\n' "$output_dir"
