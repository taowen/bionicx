#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
mkdir -p "$repo_dir/build/xfce-session-bundle/app/bin"
podman run --rm --userns=keep-id --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
    examples/xfce-session/xfce-session.c \
    -o build/xfce-session-bundle/app/bin/xfce-session -lX11 -lXtst
interpreter=/data/user/0/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
rpath=/data/user/0/io.taowen.bx/files/rootfs/usr/lib:/data/user/0/io.taowen.bx/files/rootfs/usr/lib/aarch64-linux-gnu
patchelf --set-interpreter "$interpreter" --set-rpath "$rpath" \
    "$repo_dir/build/xfce-session-bundle/app/bin/xfce-session"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/xfce-session.json" \
    --app-root "$repo_dir/build/xfce-session-bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
bionicx_log="${BIONICX_XFCE_LOG:-$repo_dir/build/xfce-session-bionicx.log}"
: > "$bionicx_log"
adb -s "$serial" logcat -v brief -s BionicX:I WinlatorXGrab:I > "$bionicx_log" &
logcat_pid=$!
trap 'kill "$logcat_pid" 2>/dev/null || true' EXIT
sleep 1
adb -s "$serial" shell am force-stop io.taowen.bx
# Thunar persists last-window-maximized and a near-screen size; xfwm4
# then ignores ConfigureRequest, which makes session-resize look like
# an X bug.
cat "$repo_dir/examples/xfce-session/thunar-geometry.xml" | \
    adb -s "$serial" shell "run-as io.taowen.bx sh -c 'mkdir -p files/homes/xfce-session/.config/xfce4/xfconf/xfce-perchannel-xml && cat > files/homes/xfce-session/.config/xfce4/xfconf/xfce-perchannel-xml/thunar.xml'"
# Debian XFCE defaults IconThemeName=Tango, but the seed only has a
# Geany Tango stub. Pin Adwaita so xfdesktop/panel see user-home.
cat "$repo_dir/examples/xfce-session/xsettings.xml" | \
    adb -s "$serial" shell "run-as io.taowen.bx sh -c 'mkdir -p files/homes/xfce-session/.config/xfce4/xfconf/xfce-perchannel-xml && cat > files/homes/xfce-session/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml'"
# Debian's xfce4-panel channel defaults dark-mode=true, which makes the
# Applications menu a dark GtkMenu in the panel process. The session
# chrome is otherwise Adwaita light.
adb -s "$serial" shell "run-as io.taowen.bx sh -c 'f=files/homes/xfce-session/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml; if [ -f \"\$f\" ]; then sed -i \"s/name=\\\"dark-mode\\\" type=\\\"bool\\\" value=\\\"true\\\"/name=\\\"dark-mode\\\" type=\\\"bool\\\" value=\\\"false\\\"/\" \"\$f\"; fi'"
adb -s "$serial" shell am start -W -n io.taowen.bx/com.winlator.BionicXActivity
mapped="${BIONICX_XFCE_MAPPED:-$repo_dir/build/xfce-session-mapped.png}"
for i in $(seq 1 80); do
    if [[ "$i" -eq 6 ]]; then
        adb -s "$serial" exec-out screencap -p > "$mapped"
    fi
    if grep -Fq 'BXSUMMARY xfce-session-accept' "$bionicx_log" \
            || adb -s "$serial" logcat -d -v brief | grep -Fq 'BXSUMMARY xfce-session-accept'; then
        echo "xfce-session accept finished at ${i}s"
        break
    fi
    sleep 1
done
adb -s "$serial" exec-out screencap -p > \
    "${BIONICX_XFCE_SCREENSHOT:-$repo_dir/build/xfce-session-device.png}"
sleep 1
kill "$logcat_pid" 2>/dev/null || true
wait "$logcat_pid" 2>/dev/null || true
trap - EXIT
log="$(cat "$bionicx_log"; adb -s "$serial" logcat -d -v brief)"
result="$(grep -E 'BXTEST|BXSUMMARY|BXINFO unimplemented|BXINFO click-|BXINFO grab-|BXINFO pre-click-|BXINFO post-click-|BXINFO bar-child|BXINFO root-stack|BXINFO find-|BXINFO saved-bar|BXINFO thin-root|BXINFO ptr-press|BXINFO grab-trace|BXINFO grab-add|BXINFO grab-add-reject|BXINFO grab-press|BXINFO grab-xi|BXINFO grab-mark|BXINFO grab-core-replay|BXINFO gdk-ev|BXINFO allow-events|BXINFO mp-paint|BXINFO mp-type|BXINFO pre-click-tip|BXINFO post-click-tip|enabled D-Bus|enabled PulseAudio|enabled app-private CUPS|enabled Vulkan' <<<"$log" | awk '!seen[$0]++')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY xfce-session-accept passed=12 failed=0" <<<"$result"
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
if grep -F "Unable to acquire bus name 'org.xfce.Thunar'" <<<"$log"; then
    echo "Thunar must own org.xfce.Thunar before xfdesktop activates it" >&2
    exit 1
fi
if grep -F "xfce4-panel" <<<"$log" | grep -Fq "BadImplementation"; then
    echo "xfce4-panel must not die on unimplemented Render" >&2
    exit 1
fi
if grep -E 'BXINFO unimplemented RENDER (SetPictureTransform|QueryFilters|CreateCursor)' \
        <<<"$log"; then
    echo "RENDER 0.6 requests must show on BionicX:I and be implemented" >&2
    exit 1
fi
if adb -s "$serial" shell run-as io.taowen.bx \
        find files/apps -name 'libc.so.6' | grep -q .; then
    echo "xfce-session must not grow a per-app libc.so.6" >&2
    exit 1
fi
echo "xfce-session two-app accept: PASS"
