#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )
package="io.taowen.bx"
cache_dir="${BIONICX_DOWNLOAD_CACHE:-$repo_dir/build/downloads}/wps-pdf"
work_dir="$(mktemp -d)"
stage_dir="$work_dir/stage"
trap 'rm -rf "$work_dir"' EXIT
mkdir -p "$cache_dir" "$stage_dir"

# These are the ARM64 Debian packages selected by Pi-Apps' WPS install-64
# baseline, plus libtiff5's two runtime dependencies that are not otherwise in
# the compact BionicX glibc tree. Hashes make the external input reproducible.
packages=(
    "libjpeg62-turbo_2.1.5-2_arm64.deb|de66f186f3ff3c1d10c2e75ae056b019b3f7f091f51096a06cade48b2dea875b|https://ftp.debian.org/debian/pool/main/libj/libjpeg-turbo/libjpeg62-turbo_2.1.5-2_arm64.deb"
    "libwebp6_0.6.1-2.1+deb11u2_arm64.deb|edeb260e528fecae77457a63a468e55837a98079fdd7f1e20e9813c358f8c755|https://ftp.debian.org/debian/pool/main/libw/libwebp/libwebp6_0.6.1-2.1+deb11u2_arm64.deb"
    "libtiff5_4.2.0-1+deb11u5_arm64.deb|6896296ef6193ff77434c5d1d09dd9a333633f7a208ab1cc7de3b286d2d45824|https://ftp.debian.org/debian/pool/main/t/tiff/libtiff5_4.2.0-1+deb11u5_arm64.deb"
    "libdeflate0_1.7-1_arm64.deb|a1adc22600ea5e44e8ea715972ac2af7994cc7ff4d94bba8e8b01abb9ddbdfd0|https://ftp.debian.org/debian/pool/main/libd/libdeflate/libdeflate0_1.7-1_arm64.deb"
    "libjbig0_2.1-3.1+b2_arm64.deb|b71b3e62e162f64cb24466bf7c6e40b05ce2a67ca7fed26d267d498f2896d549|https://ftp.debian.org/debian/pool/main/j/jbigkit/libjbig0_2.1-3.1+b2_arm64.deb"
)

extract_data_archive() {
    local archive="$1" destination="$2" member
    member="$(ar t "$archive" | grep -E '^data\.tar\.(xz|gz|zst)$' | head -1)"
    [[ -n "$member" ]] || {
        echo "missing data archive in $archive" >&2
        exit 1
    }
    case "$member" in
        *.xz) ar p "$archive" "$member" | tar -xJf - -C "$destination" ;;
        *.gz) ar p "$archive" "$member" | tar -xzf - -C "$destination" ;;
        *.zst) ar p "$archive" "$member" | tar --zstd -xf - -C "$destination" ;;
    esac
}

for specification in "${packages[@]}"; do
    IFS='|' read -r filename expected_hash url <<< "$specification"
    archive="$cache_dir/$filename"
    if [[ ! -f "$archive" ]] ||
            [[ "$(sha256sum "$archive" | cut -d' ' -f1)" != "$expected_hash" ]]; then
        curl --fail --location --retry 3 --output "$archive.partial" "$url"
        mv "$archive.partial" "$archive"
    fi
    actual_hash="$(sha256sum "$archive" | cut -d' ' -f1)"
    [[ "$actual_hash" == "$expected_hash" ]] || {
        echo "hash mismatch for $filename: $actual_hash" >&2
        exit 1
    }
    extracted="$work_dir/${filename%.deb}"
    mkdir -p "$extracted"
    extract_data_archive "$archive" "$extracted"
    printf 'BXDEB package=%s sha256=%s\n' "$filename" "$actual_hash"
done

for source_dir in "$work_dir"/*/usr/lib/aarch64-linux-gnu; do
    [[ -d "$source_dir" ]] || continue
    while IFS= read -r library; do
        cp -a "$library" "$stage_dir/"
    done < <(find "$source_dir" -maxdepth 1 \
        \( -name 'libjpeg.so.62*' -o -name 'libwebp.so.6*' \
        -o -name 'libtiff.so.5*' -o -name 'libdeflate.so.0*' \
        -o -name 'libjbig.so.0*' \) -print)
done

for soname in libjpeg.so.62 libwebp.so.6 libtiff.so.5 libdeflate.so.0 libjbig.so.0; do
    [[ -e "$stage_dir/$soname" ]] || {
        echo "missing staged PDF dependency: $soname" >&2
        exit 1
    }
    readelf -h "$stage_dir/$soname" | grep -Fq 'Machine:                           AArch64'
done

tar -C "$stage_dir" -cf - . | \
    "${adb[@]}" shell run-as "$package" tar -C files/rootfs/usr/lib -xf -

for library in "$stage_dir"/*.so.*; do
    [[ -f "$library" && ! -L "$library" ]] || continue
    filename="$(basename "$library")"
    host_hash="$(sha256sum "$library" | cut -d' ' -f1)"
    device_hash="$("${adb[@]}" exec-out run-as "$package" \
        sha256sum "files/rootfs/usr/lib/$filename" | cut -d' ' -f1)"
    [[ "$device_hash" == "$host_hash" ]] || {
        echo "device hash mismatch for $filename" >&2
        exit 1
    }
    printf 'BXELF library=%s sha256=%s\n' "$filename" "$host_hash"
done
