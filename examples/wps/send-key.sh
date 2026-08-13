#!/usr/bin/env bash
# Inject one X11 key sequence into the running BionicX display.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
files="/data/user/0/$package_id/files"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")

if [[ ! -x "$repo_dir/build/x11-send-key" ]]; then
    builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
    mkdir -p "$repo_dir/build"
    podman run --rm --network host --userns=keep-id \
        --volume "$repo_dir:/work:Z" --workdir /work \
        "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/wps/x11-send-key.c -o build/x11-send-key -lX11 -lXtst
    patchelf --set-interpreter "$root/usr/lib/ld-linux-aarch64.so.1" \
        "$repo_dir/build/x11-send-key"
fi

tmp="/data/local/tmp/x11-send-key-$$"
"${adb[@]}" push "$repo_dir/build/x11-send-key" "$tmp" >/dev/null
"${adb[@]}" shell run-as "$package_id" cp "$tmp" files/bin/x11-send-key
"${adb[@]}" shell rm -f "$tmp"

result="$("${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" --cwd "$root" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "DISPLAY=:0" \
    -- "$files/bin/x11-send-key" "$@" 2>&1 || true)"
printf '%s\n' "$result"
grep -Fq "BXTEST PASS x11-send-key" <<<"$result"
