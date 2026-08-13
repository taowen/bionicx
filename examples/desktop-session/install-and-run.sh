#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
mkdir -p "$repo_dir/build/desktop-session-bundle/app/bin"
podman run --rm --userns=keep-id --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
    examples/desktop-session/desktop-session.c \
    -o build/desktop-session-bundle/app/bin/desktop-session
patchelf --set-interpreter \
    /data/user/0/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1 \
    "$repo_dir/build/desktop-session-bundle/app/bin/desktop-session"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/desktop-session.json" \
    --app-root "$repo_dir/build/desktop-session-bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W -n io.taowen.bx/com.winlator.BionicXActivity
for i in $(seq 1 30); do
    if adb -s "$serial" logcat -d -v brief | grep -Fq 'BXTEST PASS desktop-session-launch'; then
        echo "desktop-session launched at ${i}s"
        break
    fi
    sleep 1
done
adb -s "$serial" exec-out screencap -p > \
    "${BIONICX_DESKTOP_SCREENSHOT:-$repo_dir/build/desktop-session-device.png}"
adb -s "$serial" logcat -d -v brief | grep -E 'BXTEST|BXSUMMARY|desktop-session'
