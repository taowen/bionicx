#!/usr/bin/env bash
# Launch the V2509A acceptance set and keep a screenshot of each window.
# Installing packages is not enough: each app must start untraced.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
evidence="${BIONICX_EVIDENCE:-$repo_dir/evidence/vivo-10AFA31610002QH}"
mkdir -p "$evidence"
log="$evidence/vivo-apps.log"
exec > >(tee "$log") 2>&1

echo "BXINFO serial=$serial uid=$("${adb[@]}" shell run-as "$package_id" id)"
echo "BXINFO seed=$("${adb[@]}" shell run-as "$package_id" \
    cat files/rootfs/.bionicx-rootfs-seed-id | tr -d '\r')"

require_bin() {
    local name="$1"
    "${adb[@]}" shell run-as "$package_id" test -x "files/rootfs/usr/bin/$name"
}

for bin in xterm evince keepassxc krita firefox-esr; do
    require_bin "$bin" || {
        echo "missing $bin; wait for bxapt set packages/trixie-popular.txt" >&2
        exit 1
    }
    echo "BXTEST PASS have-$bin"
done

wake() {
    "${adb[@]}" shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true
    "${adb[@]}" shell svc power stayon true >/dev/null 2>&1 || true
    "${adb[@]}" shell wm dismiss-keyguard >/dev/null 2>&1 || true
}

launch_profile() {
    local name="$1"
    local profile="$2"
    local app_root="$3"
    local shot="$evidence/${name}.png"
    echo "==== launch $name ===="
    "$repo_dir/tools/install-profile.sh" \
        --serial "$serial" \
        --profile "$profile" \
        --app-root "$app_root"
    wake
    "${adb[@]}" logcat -c
    "${adb[@]}" shell am force-stop "$package_id"
    "${adb[@]}" shell "run-as $package_id sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
    sleep 0.3
    "${adb[@]}" shell am start -W -n "$package_id/com.winlator.BionicXActivity"
    "${adb[@]}" shell cmd statusbar collapse >/dev/null 2>&1 || true
    # The first Android frame is a large gray PNG. Guest windows are smaller.
    # Wait, then keep the last shot rather than the largest.
    local i
    sleep 8
    for i in $(seq 1 20); do
        "${adb[@]}" exec-out screencap -p > "$shot"
        sleep 1
    done
    local bytes
    bytes="$(wc -c < "$shot" | tr -d ' ')"
    echo "BXSHOT $name bytes=$bytes"
    if [[ "$bytes" -le 8000 ]]; then
        echo "screenshot too small for $name" >&2
        exit 1
    fi
    "${adb[@]}" logcat -d -v brief | grep -E 'BionicX|running untraced|exited|FATAL' | tail -n 30
    if ! "${adb[@]}" logcat -d -v brief | grep -F 'running untraced' >/dev/null; then
        echo "$name did not run untraced" >&2
        exit 1
    fi
    echo "BXTEST PASS $name-untraced"
}

"$repo_dir/examples/xterm/build-bundle.sh"
launch_profile xterm \
    "$repo_dir/profiles/xterm.json" \
    "$repo_dir/build/xterm-bundle/app"

"$repo_dir/examples/productivity-apps/build-bundle.sh"
launch_profile evince \
    "$repo_dir/profiles/evince.json" \
    "$repo_dir/build/productivity-apps-bundle/app"

ANDROID_SERIAL="$serial" \
BIONICX_SCREENSHOT="$evidence/keepassxc.png" \
    "$repo_dir/examples/popular-apps/install-and-run.sh" keepassxc
if [[ ! -s "$evidence/keepassxc.png" ]]; then
    echo "missing keepassxc screenshot" >&2
    exit 1
fi
echo "BXTEST PASS keepassxc-untraced"

ANDROID_SERIAL="$serial" \
BIONICX_SCREENSHOT="$evidence/krita.png" \
    "$repo_dir/examples/popular-apps/install-and-run.sh" krita
echo "BXTEST PASS krita-untraced"

ANDROID_SERIAL="$serial" \
BIONICX_SCREENSHOT="$evidence/firefox-esr.png" \
    "$repo_dir/examples/popular-apps/install-and-run.sh" firefox-esr-online
echo "BXTEST PASS firefox-esr-untraced"

echo "BXSUMMARY vivo-apps passed=5 failed=0"
