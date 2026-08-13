#!/usr/bin/env bash
# Decode the shared-seed VLC AVI fixture through libvlc. App-only.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
files="/data/user/0/$package_id/files"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
bundle="$repo_dir/build/vlc-avi-probe-bundle"

mkdir -p "$bundle/app/bin" "$bundle/app/fixtures"
python3 "$repo_dir/examples/popular-apps/build-y4m-fixture.py" \
    "$bundle/app/fixtures/bionicx-motion.y4m" \
    "$bundle/app/fixtures/bionicx-motion.i420" \
    "$bundle/app/fixtures/bionicx-tone.wav" \
    "$bundle/app/fixtures/bionicx-motion-audio.avi"
builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/vlc-avi-probe/vlc-avi-probe.c \
        -o build/vlc-avi-probe-bundle/app/bin/vlc-avi-probe -ldl
patchelf --set-interpreter "$root/usr/lib/ld-linux-aarch64.so.1" \
    --set-rpath \
    "$root/usr/lib:$root/usr/lib/aarch64-linux-gnu:$root/lib:$root/lib/aarch64-linux-gnu" \
    "$bundle/app/bin/vlc-avi-probe"

tmp="/data/local/tmp/vlc-avi-probe-$$"
"${adb[@]}" push "$bundle/app/bin/vlc-avi-probe" "$tmp" >/dev/null
"${adb[@]}" shell run-as "$package_id" mkdir -p \
    files/apps/vlc-avi-probe/fixtures
"${adb[@]}" shell run-as "$package_id" cp "$tmp" \
    files/apps/vlc-avi-probe/vlc-avi-probe
"${adb[@]}" shell rm "$tmp"
tmp_avi="/data/local/tmp/bionicx-motion-audio-$$.avi"
"${adb[@]}" push "$bundle/app/fixtures/bionicx-motion-audio.avi" \
    "$tmp_avi" >/dev/null
"${adb[@]}" shell run-as "$package_id" cp "$tmp_avi" \
    files/apps/vlc-avi-probe/fixtures/bionicx-motion-audio.avi
"${adb[@]}" shell rm "$tmp_avi"

result="$("${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" --cwd "$root" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "BIONICX_TMPDIR=$files/run/bxapt" \
    --env "BIONICX_APP=$files/apps/vlc-avi-probe" \
    --env "VLC_PLUGIN_PATH=$root/usr/lib/aarch64-linux-gnu/vlc/plugins" \
    -- "$files/apps/vlc-avi-probe/vlc-avi-probe" 2>&1)"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY vlc-avi passed=" <<<"$result"
echo "$result" | grep -E 'BXSUMMARY vlc-avi passed=[0-9]+ failed=0' >/dev/null
echo "vlc-avi-probe: PASS"
