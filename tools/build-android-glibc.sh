#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cache_dir="$repo_dir/build/cache"
glibc_version=2.41
glibc_sha256=a5a26b22f545d6b7d7b3dd828e11e428f24f4fac43c934fb071b6a7d0828e901
package_commit=0bd35594050d283eda7b23a3d9cfa28fd11c0b15
# Winlator patches bake SYSCONFDIR as an absolute C string. relocate-prefix.py
# can retarget that only when the app prefix stays the same byte length
# (com.winlator == io.taowen.bx). A longer package such as io.taowen.ardesk
# must compile --prefix for the final path; patchelf on this libc breaks NSS.
winlator_prefix=/data/data/com.winlator/files/rootfs
target_prefix="${BIONICX_GLIBC_PREFIX:-/data/data/io.taowen.bx/files/rootfs}"
winlator_app="${winlator_prefix%/files/rootfs}"
target_app="${target_prefix%/files/rootfs}"
if [[ ${#winlator_app} -eq ${#target_app} ]]; then
    source_prefix="$winlator_prefix"
else
    source_prefix="$target_prefix"
fi
jobs="${BIONICX_GLIBC_JOBS:-8}"

verify_output() {
    python3 - "$1" "$target_prefix" <<'PY'
from pathlib import Path
import sys

output = Path(sys.argv[1])
prefix = sys.argv[2].encode()
libc = (output / "libc.so.6").read_bytes()
expected_resolver = prefix + b"/etc/resolv.conf"
if expected_resolver not in libc:
    raise SystemExit("glibc contract: fixed rootfs resolver path is absent")

# Android app seccomp traps these calls even before glibc can observe ENOSYS.
# The pinned source recipe must compile them out; runtime instruction rewriting
# is intentionally not an execution path.
svc = bytes.fromhex("010000d4")
clone3 = bytes.fromhex("683680d2")
robust = bytes.fromhex("680c80d2")
if clone3 + svc in libc:
    raise SystemExit("glibc contract: raw clone3 stub remains")
for offset in range(0, max(0, len(libc) - 16), 4):
    if libc[offset:offset + 4] == robust and svc in libc[offset + 4:offset + 16]:
        raise SystemExit("glibc contract: raw set_robust_list stub remains")
PY
}

mkdir -p "$cache_dir"
definition_hash="$({
    printf '%s\n' "$glibc_version" "$glibc_sha256" "$package_commit" \
        "$source_prefix" "$target_prefix"
    sha256sum "$repo_dir/tools/container/Containerfile.glibc-arm64" \
        "$repo_dir/runtime/glibc/2.41/zz-bionicx-robust-fallback.patch" \
        "$repo_dir/runtime/glibc/2.41/zz-android-group-members.patch" \
        "$0" | cut -d ' ' -f1
} | sha256sum | cut -c1-16)"
result_dir="$cache_dir/android-glibc-$definition_hash"
if [[ -x "$result_dir/output/ld-linux-aarch64.so.1" \
        && -f "$result_dir/output/libc.so.6" \
        && -f "$result_dir/output/libm.so.6" ]]; then
    verify_output "$result_dir/output"
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
    "$repo_dir/runtime/glibc/2.41/zz-bionicx-robust-fallback.patch"

linux_dir="$temporary/source/sysdeps/unix/sysv/linux"
cp "$temporary/package"/shm{at,ctl,dt,get}.c \
    "$temporary/package"/mprotect.c "$temporary/package"/syscall.c \
    "$temporary/package"/fakesyscall*.h \
    "$temporary/package"/fake_epoll_pwait2.c \
    "$temporary/package"/setfs{u,g}id.c "$linux_dir/"
cp "$temporary/package"/shmem-android.{c,h} \
    "$temporary/source/sysvipc/"
cp "$temporary/package/syslog.c" "$temporary/source/misc/"
mv "$linux_dir/aarch64/clone3.S" "$linux_dir/aarch64/clone3.S.disabled"
mv "$linux_dir/aarch64/syscall.S" "$linux_dir/aarch64/syscallS.S"

cp "$temporary/package"/android_passwd_group.{c,h} \
    "$temporary/package"/android_system_user_ids.h "$temporary/source/nss/"
apply_source_patch \
    "$repo_dir/runtime/glibc/2.41/zz-android-group-members.patch"
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
done < <(jq -r '.[] | .[]' "$temporary/package/fakesyscall.json")
{
    printf '\n#define DISABLED_SYSCALL_WITH_FAKESYSCALL \\\n'
    while IFS= read -r fake; do
        need_return=false
        while IFS= read -r syscall_name; do
            if grep -q "#define __NR_${syscall_name} " "$disabled_header"; then
                printf '\tcase __NR_%s: \\\n' "$syscall_name"
                need_return=true
            elif [[ "$syscall_name" =~ ^[0-9]+$ ]]; then
                printf '\tcase %s: \\\n' "$syscall_name"
                need_return=true
            fi
        done < <(jq -r --arg fake "$fake" '.[$fake][]' \
            "$temporary/package/fakesyscall.json")
        if [[ "$need_return" == true ]]; then
            printf '\t\treturn %s; \\\n' "$fake"
        fi
    done < <(jq -r 'keys[]' "$temporary/package/fakesyscall.json")
} >> "$disabled_header"
sed -i '$ s| \\$||' "$disabled_header"

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
if [[ "$source_prefix" != "$target_prefix" ]]; then
    "$repo_dir/tools/relocate-prefix.py" "$temporary/output" \
        --from-prefix "${source_prefix%/files/rootfs}" \
        --to-prefix "${target_prefix%/files/rootfs}"
fi
verify_output "$temporary/output"

printf '%s\n' \
    "glibc=$glibc_version" \
    "glibc_sha256=$glibc_sha256" \
    "glibc_packages_commit=$package_commit" \
    "build_definition=$definition_hash" > "$temporary/output/BUILD-INFO"

mv "$temporary" "$result_dir"
trap - EXIT
echo "$result_dir/output"
