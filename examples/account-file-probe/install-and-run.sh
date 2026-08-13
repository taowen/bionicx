#!/usr/bin/env bash
# Build and run the account-file probe on the device without replacing rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
files="/data/user/0/$package_id/files"
adb=("${ADB:-adb}" -s "$serial")
builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
probe="$repo_dir/build/account-file-probe"

mkdir -p "$repo_dir/build"
podman run --rm --userns=keep-id --volume "$repo_dir:/work:Z" \
    --workdir /work "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
    examples/account-file-probe/account-file-probe.c \
    -o build/account-file-probe -ldl
patchelf --set-interpreter "$root/usr/lib/ld-linux-aarch64.so.1" "$probe"

temporary="/data/local/tmp/account-file-probe-$$"
"${adb[@]}" push "$probe" "$temporary" >/dev/null
"${adb[@]}" shell run-as "$package_id" mkdir -p files/apps/account-file-probe
"${adb[@]}" shell run-as "$package_id" cp "$temporary" \
    files/apps/account-file-probe/account-file-probe
"${adb[@]}" shell rm "$temporary"

"${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" \
    --cwd "$root" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "BIONICX_TMPDIR=$files/run/bxapt" \
    --env "BIONICX_VIRTUAL_ROOT=1" \
    --env "BIONICX_REWRITE_ABSOLUTE_SYMLINKS=1" \
    -- "$files/apps/account-file-probe/account-file-probe"
