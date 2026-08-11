#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cache_dir="$repo_dir/build/cache"
glibc_version=2.39
glibc_sha256=f77bd47cf8170c57365ae7bf86696c118adb3b120d3259c64c502d3dc1e2d926
package_commit=e2ffc0bb462177386b44ec66e30e6e939d846871
source_prefix=/data/data/com.winlator/files/rootfs
target_prefix=/data/data/io.taowen.bx/files/rootfs
jobs="${BIONICX_GLIBC_JOBS:-8}"

mkdir -p "$cache_dir"
definition_hash="$({
    printf '%s\n' "$glibc_version" "$glibc_sha256" "$package_commit"
    sha256sum "$repo_dir/tools/container/Containerfile.glibc-arm64" \
        "$repo_dir/runtime/glibc/2.39/zz-bionicx-robust-fallback.patch" \
        "$repo_dir/runtime/glibc/2.39/post-prepare-gcc14.patch" \
        "$0" | cut -d ' ' -f1
} | sha256sum | cut -c1-16)"
result_dir="$cache_dir/android-glibc-$definition_hash"
if [[ -x "$result_dir/output/ld-linux-aarch64.so.1" \
        && -f "$result_dir/output/libc.so.6" \
        && -f "$result_dir/output/libm.so.6" ]]; then
    echo "$result_dir/output"
    exit 0
fi

archive="$cache_dir/glibc-$glibc_version.tar.xz"
if [[ ! -f "$archive" ]]; then
    curl -fL "https://ftp.gnu.org/gnu/libc/glibc-$glibc_version.tar.xz" \
        -o "$archive"
fi
echo "$glibc_sha256  $archive" | sha256sum -c -

package_repo="$cache_dir/glibc-packages"
if [[ ! -d "$package_repo/.git" ]]; then
    git clone https://github.com/termux-pacman/glibc-packages.git "$package_repo"
fi
if ! git -C "$package_repo" cat-file -e "$package_commit^{commit}" 2>/dev/null; then
    git -C "$package_repo" fetch --depth 1 origin "$package_commit"
fi
actual_commit="$(git -C "$package_repo" rev-parse "$package_commit^{commit}")"
[[ "$actual_commit" == "$package_commit" ]] || {
    echo "unexpected glibc-packages commit: $actual_commit" >&2
    exit 1
}

temporary="$(mktemp -d "$cache_dir/android-glibc-$definition_hash.XXXXXXXX")"
cleanup() {
    case "$temporary" in
        "$cache_dir"/android-glibc-*) rm -rf -- "$temporary" ;;
        *) echo "refusing to clean unexpected path: $temporary" >&2 ;;
    esac
}
trap cleanup EXIT
mkdir -p "$temporary/source" "$temporary/package" "$temporary/build" \
    "$temporary/output"
tar -xJf "$archive" -C "$temporary/source" --strip-components=1
git -C "$package_repo" archive "$package_commit" gpkg/glibc | \
    tar -x -C "$temporary/package" --strip-components=2

apply_source_patch() {
    local patch_file="$1"
    sed -e "s|@TERMUX_PREFIX_CLASSICAL@|$source_prefix|g" \
        -e "s|@TERMUX_PREFIX@|$source_prefix|g" "$patch_file" | \
        patch -d "$temporary/source" -p1 --forward --batch
}
for patch_file in "$temporary/package"/*.patch; do
    apply_source_patch "$patch_file"
done
apply_source_patch \
    "$repo_dir/runtime/glibc/2.39/zz-bionicx-robust-fallback.patch"

linux_dir="$temporary/source/sysdeps/unix/sysv/linux"
cp "$temporary/package"/shm{at,ctl,dt,get}.c \
    "$temporary/package"/mprotect.c \
    "$temporary/package"/syscall.c \
    "$temporary/package"/fake-syscall.h \
    "$temporary/package"/shmem-android.h "$linux_dir/"
mv "$linux_dir/aarch64/clone3.S" "$linux_dir/aarch64/clone3.S.disabled"
mv "$linux_dir/aarch64/syscall.S" "$linux_dir/aarch64/syscallS.S"

cp "$temporary/package"/android_passwd_group.{c,h} \
    "$temporary/package"/android_system_user_ids.h "$temporary/source/nss/"
bash "$temporary/package/gen-android-ids.sh" "$source_prefix" \
    "$temporary/source/nss/android_ids.h" \
    "$temporary/package/android_system_user_ids.h"

disabled_header="$linux_dir/aarch64/disabled-syscall.h"
touch "$disabled_header"
while read -r syscall_name; do
    grep "#define __NR_${syscall_name} " \
        "$linux_dir/aarch64/arch-syscall.h" >> "$disabled_header" || true
    sed -i "/#define __NR_${syscall_name} /d" \
        "$linux_dir/aarch64/arch-syscall.h"
done < "$temporary/package/disabled-syscalls"
patch -d "$temporary/source" -p1 --forward --batch < \
    "$repo_dir/runtime/glibc/2.39/post-prepare-gcc14.patch"

printf '%s\n' \
    "slibdir=$source_prefix/lib" \
    "rtlddir=$source_prefix/lib" \
    "sbindir=$source_prefix/bin" \
    "rootsbindir=$source_prefix/bin" > "$temporary/build/configparms"

builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
container_dir="/work/${temporary#"$repo_dir/"}"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir "$container_dir/build" \
    "$builder_image" "$container_dir/source/configure" \
        --prefix="$source_prefix" --libdir="$source_prefix/lib" \
        --libexecdir="$source_prefix/lib" \
        --host=aarch64-linux-gnu --build=x86_64-linux-gnu \
        --enable-kernel=3.7 --enable-bind-now --disable-multi-arch \
        --enable-stack-protector=strong --disable-nscd --disable-profile \
        --disable-werror --disable-default-pie
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir "$container_dir/build" \
    "$builder_image" gmake --silent -j"$jobs"

cp "$temporary/build/libc.so" "$temporary/output/libc.so.6"
cp "$temporary/build/elf/ld.so" \
    "$temporary/output/ld-linux-aarch64.so.1"
cp "$temporary/build/math/libm.so" "$temporary/output/libm.so.6"
podman run --rm --userns=keep-id --volume "$repo_dir:/work:Z" \
    --workdir /work "$builder_image" aarch64-linux-gnu-strip --strip-unneeded \
    "$container_dir/output/libc.so.6" \
    "$container_dir/output/ld-linux-aarch64.so.1" \
    "$container_dir/output/libm.so.6"
"$repo_dir/tools/relocate-prefix.py" "$temporary/output" \
    --from-prefix "${source_prefix%/files/rootfs}" \
    --to-prefix "${target_prefix%/files/rootfs}"

printf '%s\n' \
    "glibc=$glibc_version" \
    "glibc_sha256=$glibc_sha256" \
    "glibc_packages_commit=$package_commit" \
    "build_definition=$definition_hash" > "$temporary/output/BUILD-INFO"

mv "$temporary" "$result_dir"
trap - EXIT
echo "$result_dir/output"
