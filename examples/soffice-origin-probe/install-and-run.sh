#!/usr/bin/env bash
# LibreOffice $ORIGIN/libreglo.so through the multiarch cppuhelper symlink.
# App-only: the fixture/probe must not replace the device seed.
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
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
    examples/soffice-origin-probe/soffice-origin-probe.c \
    -o build/soffice-origin-probe -ldl

patchelf --set-interpreter \
    "$root/usr/lib/ld-linux-aarch64.so.1" \
    --set-rpath \
    "$root/usr/lib:$root/usr/lib/aarch64-linux-gnu:$root/lib:$root/lib/aarch64-linux-gnu" \
    "$repo_dir/build/soffice-origin-probe"

tmp="/data/local/tmp/soffice-origin-probe-$$"
"${adb[@]}" push "$repo_dir/build/soffice-origin-probe" "$tmp" >/dev/null
"${adb[@]}" shell run-as "$package_id" mkdir -p files/apps/soffice-origin-probe
"${adb[@]}" shell run-as "$package_id" cp "$tmp" \
    files/apps/soffice-origin-probe/soffice-origin-probe
"${adb[@]}" shell rm "$tmp"

result="$("${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" --cwd "$root" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "BIONICX_TMPDIR=$files/run/bxapt" \
    -- "$files/apps/soffice-origin-probe/soffice-origin-probe" 2>&1)"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY soffice-origin passed=" <<<"$result"
echo "$result" | grep -E 'BXSUMMARY soffice-origin passed=[0-9]+ failed=0' >/dev/null

helper_runpath="$("${adb[@]}" shell run-as "$package_id" \
    "$root/usr/bin/readelf" -d \
    "$root/usr/lib/libreoffice/program/libuno_cppuhelpergcc3.so.3")"
printf '%s\n' "$helper_runpath"
echo "$helper_runpath" | grep -F "(RUNPATH)" | grep -Fq \
    "$root/usr/lib/libreoffice/program:"

version="$("${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" \
    --cwd "$root/usr/lib/libreoffice/program" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "BIONICX_TMPDIR=$files/run/bxapt" \
    --env "HOME=$files/homes/libreoffice-writer" \
    --env "TMPDIR=$files/run/bxapt" \
    --env "SAL_DISABLE_OPENCL=1" \
    --env "SAL_USE_VCLPLUGIN=svp" \
    -- "$root/usr/lib/libreoffice/program/soffice.bin" --headless --version 2>&1)"
printf '%s\n' "$version"
grep -Eiq 'LibreOffice' <<<"$version"

init="$("${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" \
    --cwd "$root/usr/lib/libreoffice/program" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "BIONICX_TMPDIR=$files/run/bxapt" \
    --env "HOME=$files/homes/libreoffice-writer" \
    --env "TMPDIR=$files/run/bxapt" \
    --env "SAL_DISABLE_OPENCL=1" \
    --env "SAL_USE_VCLPLUGIN=svp" \
    -- "$root/usr/lib/libreoffice/program/soffice.bin" \
    --headless --terminate_after_init 2>&1)"
printf '%s\n' "$init"
echo "$init" | grep -Fq 'exited status=0x0'
echo "soffice-origin-probe: PASS"
