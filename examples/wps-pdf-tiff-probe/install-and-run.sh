#!/usr/bin/env bash
# Install hash-pinned bullseye libwebp6/libtiff5 into the shared rootfs, then
# prove wpspdf's libpdfmain.so can dlopen libtiff.so.5 from that tree.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
files="/data/user/0/$package_id/files"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")

"$repo_dir/examples/wps-pdf-tiff-probe/fetch-debs.sh"
while IFS=$'\t' read -r package version sha256 url; do
    [[ "$package" == libwebp6 || "$package" == libtiff5 ]] || continue
    archive="$repo_dir/build/cache/${package}-${version}/${url##*/}"
    "$repo_dir/tools/bxapt" --serial "$serial" deb "$archive" "$sha256"
done < <(grep -E '^(libwebp6|libtiff5)' "$repo_dir/packages/external-arm64.tsv")

builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
mkdir -p "$repo_dir/build"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
    examples/wps-pdf-tiff-probe/wps-pdf-tiff-probe.c \
    -o build/wps-pdf-tiff-probe -ldl

patchelf --set-interpreter \
    "$root/usr/lib/ld-linux-aarch64.so.1" \
    "$repo_dir/build/wps-pdf-tiff-probe"

tmp="/data/local/tmp/wps-pdf-tiff-probe-$$"
"${adb[@]}" push "$repo_dir/build/wps-pdf-tiff-probe" "$tmp" >/dev/null
"${adb[@]}" shell run-as "$package_id" mkdir -p files/apps/wps-pdf-tiff-probe
"${adb[@]}" shell run-as "$package_id" cp "$tmp" \
    files/apps/wps-pdf-tiff-probe/wps-pdf-tiff-probe
"${adb[@]}" shell rm "$tmp"

result="$("${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" --cwd "$root" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "BIONICX_TMPDIR=$files/run/bxapt" \
    -- "$files/apps/wps-pdf-tiff-probe/wps-pdf-tiff-probe" 2>&1 || true)"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY wps-pdf-tiff passed=5 failed=0" <<<"$result"

status="$("$repo_dir/tools/bxapt" --serial "$serial" query -W \
    -f '${Package} ${Version} ${Status}\n' libtiff5 libwebp6)"
printf '%s\n' "$status"
grep -E '^libtiff5 4\.2\.0-1\+deb11u5 install ok installed$' <<<"$status"
grep -E '^libwebp6 0\.6\.1-2\.1\+deb11u2 install ok installed$' <<<"$status"
if "${adb[@]}" shell run-as "$package_id" \
        find files/apps -name 'libtiff.so.5' -o -name 'libtiff.so.5.*' \
        | grep -q .; then
    echo "libtiff.so.5 must live in the shared rootfs, not files/apps" >&2
    exit 1
fi
echo "wps pdf tiff probe: PASS"
