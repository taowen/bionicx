#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
mkdir -p "$repo_dir/build/xfce-session-bundle/app/bin"
podman run --rm --userns=keep-id --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
    examples/xfce-session/xfce-session.c \
    -o build/xfce-session-bundle/app/bin/xfce-session -lX11
interpreter=/data/user/0/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
rpath=/data/user/0/io.taowen.bx/files/rootfs/usr/lib:/data/user/0/io.taowen.bx/files/rootfs/usr/lib/aarch64-linux-gnu
patchelf --set-interpreter "$interpreter" --set-rpath "$rpath" \
    "$repo_dir/build/xfce-session-bundle/app/bin/xfce-session"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/xfce-session.json" \
    --app-root "$repo_dir/build/xfce-session-bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
# Thunar persists last-window-maximized and a near-screen size; xfwm4
# then ignores ConfigureRequest, which makes session-resize look like
# an X bug.
cat "$repo_dir/examples/xfce-session/thunar-geometry.xml" | \
    adb -s "$serial" shell "run-as io.taowen.bx sh -c 'mkdir -p files/homes/xfce-session/.config/xfce4/xfconf/xfce-perchannel-xml && cat > files/homes/xfce-session/.config/xfce4/xfconf/xfce-perchannel-xml/thunar.xml'"
adb -s "$serial" shell am start -W -n io.taowen.bx/com.winlator.BionicXActivity
mapped="${BIONICX_XFCE_MAPPED:-$repo_dir/build/xfce-session-mapped.png}"
for i in $(seq 1 80); do
    if [[ "$i" -eq 6 ]]; then
        adb -s "$serial" exec-out screencap -p > "$mapped"
    fi
    if adb -s "$serial" logcat -d -v brief | grep -Fq 'BXSUMMARY xfce-session-accept'; then
        echo "xfce-session accept finished at ${i}s"
        break
    fi
    sleep 1
done
adb -s "$serial" exec-out screencap -p > \
    "${BIONICX_XFCE_SCREENSHOT:-$repo_dir/build/xfce-session-device.png}"
log="$(adb -s "$serial" logcat -d -v brief)"
result="$(grep -E 'BXTEST|BXSUMMARY|enabled D-Bus|enabled PulseAudio|enabled app-private CUPS|enabled Vulkan' <<<"$log")"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY xfce-session-accept passed=11 failed=0" <<<"$result"
grep -Fq "enabled D-Bus session service" <<<"$result"
if grep -F 'Conversion from ISO-8859-1 to UTF-8 is not supported' <<<"$log"; then
    echo "xfce-session must convert latin1 clipboard text" >&2
    exit 1
fi
if grep -F 'cannot open display' <<<"$log"; then
    echo "D-Bus-activated GTK apps must inherit DISPLAY" >&2
    exit 1
fi
if grep -F 'does not support the XRes extension' <<<"$log"; then
    echo "xfwm4 must see X-Resource" >&2
    exit 1
fi
if grep -F 'does not support the XSync extension' <<<"$log"; then
    echo "xfwm4 must see SYNC" >&2
    exit 1
fi
if grep -F 'Unsupported keyboard modifier' <<<"$log"; then
    echo "xfwm4 must map Super/Mod4" >&2
    exit 1
fi
if grep -F 'XRandR initialization error' <<<"$log"; then
    echo "xfwm4 must see RandR 1.5" >&2
    exit 1
fi
if adb -s "$serial" shell run-as io.taowen.bx \
        find files/apps -name 'libc.so.6' | grep -q .; then
    echo "xfce-session must not grow a per-app libc.so.6" >&2
    exit 1
fi
echo "xfce-session two-app accept: PASS"
