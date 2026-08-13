#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="${1:?usage: tools/build-rootfs-seed.sh OUTPUT_BUNDLE_DIR}"
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
install_script="$repo_dir/tools/install-trixie-rootfs-seed.sh"
input_id="$({
    printf '%s\n%s\n' "$base_image" "$debian_snapshot"
    # Hash content only: cache identity must not depend on the clone's absolute
    # host path (sha256sum normally includes the filename in its output).
    sha256sum "$install_script" | cut -d' ' -f1
} | sha256sum | cut -d' ' -f1)"
image="localhost/bionicx-trixie-seed:$input_id"
container="bionicx-trixie-seed-${input_id:0:12}-$$"
temporary="$(mktemp -d "$repo_dir/build/trixie-rootfs.XXXXXXXX")"
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
        --env http_proxy= --env https_proxy= \
        --env HTTP_PROXY= --env HTTPS_PROXY= \
        "$base_image" /bin/sh /tmp/install-bionicx-seed.sh >/dev/null
    podman cp "$install_script" "$container:/tmp/install-bionicx-seed.sh"
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
mkdir -p "$output_dir/rootfs"
cp -a "$temporary/exported/etc" "$temporary/exported/usr" \
    "$temporary/exported/var" "$output_dir/rootfs/"
for merged_path in bin lib lib64 sbin; do
    [[ ! -e "$temporary/exported/$merged_path" ]] || \
        cp -a "$temporary/exported/$merged_path" "$output_dir/rootfs/"
done
cp -a "$temporary/exported/bionicx/metadata/." "$output_dir/"
podman unshare chown -R 0:0 "$output_dir"
chmod u+w "$output_dir/rootfs/etc/machine-id"
printf '%s' '21eeeb73de5941e9a77d41bb0b9953d5' \
    > "$output_dir/rootfs/etc/machine-id"

# Debian packages intentionally use absolute FHS symlinks, especially through
# dpkg alternatives.  With no mount namespace/chroot those links would escape
# the app-private image and resolve against Android.  Preserve their targets
# while rewriting every internally resolvable absolute link to a relative one.
while IFS= read -r -d '' link; do
    target="$(readlink "$link")"
    [[ "$target" == /* ]] || continue
    rooted_target="$output_dir/rootfs$target"
    [[ -e "$rooted_target" || -L "$rooted_target" ]] || continue
    relative="$(realpath -m --relative-to="$(dirname "$link")" "$rooted_target")"
    ln -sfn "$relative" "$link"
done < <(find "$output_dir/rootfs" -type l -print0)

# Overlay the Android-seccomp/path-compatible loader and libc at the first
# library-search location.  Debian's original multiarch files remain intact as
# package-owned data; BionicX processes always enter through this loader.
runtime_overlay="$temporary/runtime-overlay"
"$repo_dir/examples/hello/build-bundle.sh" "$runtime_overlay"
rm -f "$runtime_overlay/app/bin/hello-x11"
# Debian exposes its loader through a multiarch symlink. BionicX's fixed
# interpreter is a separate build artifact and must not overwrite that symlink
# target or inherit its pathname identity.
rm -f "$output_dir/rootfs/usr/lib/ld-linux-aarch64.so.1"
cp -a "$runtime_overlay/rootfs/usr/lib/." "$output_dir/rootfs/usr/lib/"
[[ -f "$output_dir/rootfs/usr/lib/ld-linux-aarch64.so.1" &&
        ! -L "$output_dir/rootfs/usr/lib/ld-linux-aarch64.so.1" ]]
"$repo_dir/tools/build-gladio.sh" "$output_dir/rootfs/usr/lib"

# Seed construction and every bxapt transaction use the same ELF
# normalization implementation.  The host root is merely the source tree;
# every deployed absolute interpreter/RUNPATH names the canonical device root.
device_root=/data/user/0/io.taowen.bx/files/rootfs
BIONICX_PATCHELF=patchelf BIONICX_READELF=readelf \
BIONICX_INTERPRETER="$device_root/usr/lib/ld-linux-aarch64.so.1" \
BIONICX_DEPLOY_ROOT="$device_root" BIONICX_ROOT_ALIAS="$device_root" \
    "$repo_dir/tools/rootfs-elf-fixup.sh" "$output_dir/rootfs"

# The Android kernel resolves script interpreters before the glibc loader or
# LD_PRELOAD can run.  Relocate packaged FHS shebangs for the same reason as
# PT_INTERP; this covers /usr/bin/env-based Python plug-ins and shell helpers.
"$repo_dir/tools/relocate-shebangs.py" "$output_dir/rootfs" \
    --device-root "$device_root"

"$repo_dir/tools/check-glibc-symbol-floor.py" "$output_dir/rootfs" --maximum 2.41

manifest="$output_dir/rootfs/.bionicx-rootfs-seed.manifest"
{
    printf 'distribution=debian-13-trixie\n'
    printf 'debian_snapshot=%s\n' "$debian_snapshot"
    printf 'base_image=%s\n' "$base_image"
    printf 'package_manifest_sha256=%s\n' "$(sha256sum "$output_dir/packages.tsv" | cut -d' ' -f1)"
    (cd "$output_dir/rootfs" && find . -type f \
        ! -name '.bionicx-rootfs-seed.manifest' \
        ! -name '.bionicx-rootfs-seed-id' -exec sha256sum {} + | sort -k2)
} > "$manifest"
sha256sum "$manifest" | cut -d' ' -f1 \
    > "$output_dir/rootfs/.bionicx-rootfs-seed-id"
printf '%s\n' "$output_dir"
