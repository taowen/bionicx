#!/usr/bin/env bash
# Debian libvlc.so.5 plus the XCB video plugin. App-only; no AVI playback.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
files="/data/user/0/$package_id/files"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
bundle="$repo_dir/build/vlc-plugin-probe-bundle"

mkdir -p "$bundle/app/bin"
builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/vlc-plugin-probe/vlc-plugin-probe.c \
        -o build/vlc-plugin-probe-bundle/app/bin/vlc-plugin-probe -ldl
patchelf --set-interpreter "$root/usr/lib/ld-linux-aarch64.so.1" \
    --set-rpath \
    "$root/usr/lib:$root/usr/lib/aarch64-linux-gnu:$root/lib:$root/lib/aarch64-linux-gnu" \
    "$bundle/app/bin/vlc-plugin-probe"

tmp="/data/local/tmp/vlc-plugin-probe-$$"
"${adb[@]}" push "$bundle/app/bin/vlc-plugin-probe" "$tmp" >/dev/null
"${adb[@]}" shell chmod 644 "$tmp"
"${adb[@]}" shell run-as "$package_id" mkdir -p files/apps/vlc-plugin-probe
"${adb[@]}" shell run-as "$package_id" cp "$tmp" \
    files/apps/vlc-plugin-probe/vlc-plugin-probe
"${adb[@]}" shell rm "$tmp"

result="$("${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" --cwd "$root" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "BIONICX_TMPDIR=$files/run/bxapt" \
    --env "VLC_PLUGIN_PATH=$root/usr/lib/aarch64-linux-gnu/vlc/plugins" \
    -- "$files/apps/vlc-plugin-probe/vlc-plugin-probe" 2>&1)"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY vlc-plugin passed=" <<<"$result"
echo "$result" | grep -E 'BXSUMMARY vlc-plugin passed=[0-9]+ failed=0' >/dev/null
echo "vlc-plugin-probe: PASS"
