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
compat="$temporary/compat"
"$repo_dir/examples/hello/build-bundle.sh" "$compat"
rm -f "$compat/app/bin/hello-x11"
cp -a "$compat/rootfs/usr/lib/." "$output_dir/rootfs/usr/lib/"
"$repo_dir/tools/build-gladio.sh" "$output_dir/rootfs/usr/lib"

# Any Debian program can execute another packaged ELF (shells, GIMP plug-ins,
# media helpers, and so on).  With no chroot or PRoot, the kernel cannot resolve
# their stock /lib/ld-linux path.  Adapt every executable carrying PT_INTERP to
# the app-private loader, while leaving shared objects and scripts untouched.
# dpkg's package database remains available to audit the compatibility overlay.
device_loader=/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
patched_interpreters=0
while IFS= read -r -d '' executable; do
    interpreter="$(patchelf --print-interpreter "$executable" 2>/dev/null || true)"
    [[ -n "$interpreter" ]] || continue
    [[ "$interpreter" == "$device_loader" ]] || \
        patchelf --set-interpreter "$device_loader" "$executable"
    ((patched_interpreters += 1))
done < <(find "$output_dir/rootfs" -type f -perm /111 -print0)
printf 'patched %d executable ELF interpreters\n' "$patched_interpreters"

# DT_RPATH/DT_RUNPATH entries from Debian packages are FHS paths too.  The
# dynamic loader cannot resolve an entry such as /usr/lib/.../vlc without a
# mount namespace, even though the package and its private libraries are both
# present in the shared image.  Relocate every absolute search component once
# at rootfs build time; keep $ORIGIN and other relative components unchanged.
device_root=/data/data/io.taowen.bx/files/rootfs
relocated_runpaths=0
while IFS= read -r -d '' elf; do
    old_runpath="$(patchelf --print-rpath "$elf" 2>/dev/null || true)"
    [[ -n "$old_runpath" ]] || continue
    IFS=: read -r -a components <<< "$old_runpath"
    new_runpath=
    for component in "${components[@]}"; do
        case "$component" in
            /usr/*|/lib|/lib/*|/lib64|/lib64/*)
                component="$device_root$component"
                ;;
        esac
        [[ -z "$new_runpath" ]] || new_runpath+=:
        new_runpath+="$component"
    done
    [[ "$new_runpath" == "$old_runpath" ]] || {
        patchelf --set-rpath "$new_runpath" "$elf"
        ((relocated_runpaths += 1))
    }
done < <(find "$output_dir/rootfs" -type f -print0)
printf 'relocated %d absolute ELF runpaths\n' "$relocated_runpaths"

# Debian's generated pixbuf cache names modules by absolute FHS paths.  There
# is intentionally no chroot/proot on Android, so relocate those text entries
# to the app-private runtime prefix while retaining Debian's directory layout.
pixbuf_cache="$output_dir/rootfs/usr/lib/aarch64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders.cache"
if [[ -f "$pixbuf_cache" ]]; then
    device_root=/data/data/io.taowen.bx/files/rootfs
    sed -i "s|\"/usr/lib/|\"$device_root/usr/lib/|g" "$pixbuf_cache"
fi

# Fontconfig is another package service whose configuration contains absolute
# FHS paths. Keep Debian's enabled conf.d policy, but make it self-contained
# and relocate only filesystem locations into the immutable Android rootfs.
fontconfig_root="$output_dir/rootfs/etc/fonts"
if [[ -f "$fontconfig_root/fonts.conf" ]]; then
    device_root=/data/data/io.taowen.bx/files/rootfs
    sed -i \
        -e "s|<dir>/usr/share/fonts</dir>|<dir>$device_root/usr/share/fonts</dir>|" \
        -e "s|<dir>/usr/local/share/fonts</dir>|<dir>$device_root/usr/local/share/fonts</dir>|" \
        -e "s|<cachedir>/var/cache/fontconfig</cachedir>|<cachedir>$device_root/var/cache/fontconfig</cachedir>|" \
        "$fontconfig_root/fonts.conf"
    for config in "$fontconfig_root/conf.d"/*; do
        target="$(readlink "$config" || true)"
        [[ "$target" == /usr/share/fontconfig/conf.avail/* ]] || continue
        source="$output_dir/rootfs$target"
        [[ -f "$source" ]] || {
            echo "missing Fontconfig policy target: $target" >&2
            exit 1
        }
        rm "$config"
        cp "$source" "$config"
    done
fi

# LibreOffice resolves its component registry through file: URLs stored in
# package-owned bootstrap files.  Those URLs bypass the dynamic loader and
# therefore need the same app-private FHS relocation as symlinks, RUNPATH and
# Fontconfig.  Keep the package layout intact and change only absolute URLs.
libreoffice_program="$output_dir/rootfs/usr/lib/libreoffice/program"
if [[ -d "$libreoffice_program" ]]; then
    device_root=/data/data/io.taowen.bx/files/rootfs
    for bootstrap in "$libreoffice_program"/*rc; do
        [[ -f "$bootstrap" ]] || continue
        sed -i \
            -e "s|file:///usr/|file://$device_root/usr/|g" \
            -e "s|file:///etc/|file://$device_root/etc/|g" \
            "$bootstrap"
    done
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
