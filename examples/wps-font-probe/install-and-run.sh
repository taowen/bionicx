#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
files="/data/user/0/$package_id/files"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")

builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
mkdir -p "$repo_dir/build"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" sh -eu -c '
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            examples/wps-font-probe/wps-font-probe.c \
            -o build/wps-font-probe \
            -lfontconfig
    '
patchelf --set-interpreter \
    "$root/usr/lib/ld-linux-aarch64.so.1" \
    "$repo_dir/build/wps-font-probe"

tmp="/data/local/tmp/wps-font-probe-$$"
"${adb[@]}" push "$repo_dir/build/wps-font-probe" "$tmp" >/dev/null
"${adb[@]}" push \
    "$repo_dir/examples/wps-font-probe/30-bionicx-liberation-aliases.conf" \
    "${tmp}.conf" >/dev/null
"${adb[@]}" shell run-as "$package_id" mkdir -p files/apps/wps-font-probe \
    files/rootfs/etc/fonts/conf.d
"${adb[@]}" shell run-as "$package_id" cp "$tmp" \
    files/apps/wps-font-probe/wps-font-probe
"${adb[@]}" shell run-as "$package_id" cp "${tmp}.conf" \
    files/rootfs/etc/fonts/conf.d/50-bionicx-liberation-aliases.conf
"${adb[@]}" shell rm "$tmp" "${tmp}.conf"

result="$("${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" --cwd "$root" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "BIONICX_TMPDIR=$files/run/bxapt" \
    --env "FONTCONFIG_PATH=$root/etc/fonts" \
    --env "FONTCONFIG_FILE=fonts.conf" \
    --env "FONTCONFIG_SYSROOT=$root" \
    -- "$files/apps/wps-font-probe/wps-font-probe")"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY wps-font-families passed=6 failed=0" <<<"$result"
