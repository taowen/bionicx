#!/usr/bin/env bash
# Install one popular/productivity profile onto the shared seed and launch it.
# App-only install. The fixture bundle must not replace the device seed.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
profile_name="${1:?usage: $0 firefox-esr-online|krita|qbittorrent|keepassxc}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
package_id=io.taowen.bx

case "$profile_name" in
    firefox-esr-online|krita)
        bundle_dir="${BIONICX_PRODUCTIVITY_BUNDLE:-$repo_dir/build/productivity-apps-bundle}"
        "$repo_dir/examples/productivity-apps/build-bundle.sh" "$bundle_dir"
        if [[ "$profile_name" == krita ]]; then
            mkdir -p "$bundle_dir/app/lib"
            "$repo_dir/tools/build-gladio.sh" "$bundle_dir/app/lib"
        fi
        ;;
    qbittorrent|keepassxc)
        bundle_dir="${BIONICX_POPULAR_BUNDLE:-$repo_dir/build/popular-apps-bundle}"
        "$repo_dir/examples/popular-apps/build-bundle.sh" "$bundle_dir"
        ;;
    *)
        echo "unknown profile: $profile_name" >&2
        exit 2
        ;;
esac

"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/${profile_name}.json" \
    --app-root "$bundle_dir/app" \
    --serial "$serial"

if [[ "$profile_name" == keepassxc ]]; then
    "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh"
fi

if [[ "$profile_name" == firefox-esr-online ]]; then
    # GreD loads libnssckbi.so by full path. Reuse the shared libnss3 copy.
    if "${adb[@]}" shell run-as "$package_id" \
            test -e files/rootfs/usr/lib/firefox-esr/libsoftokn3.so &&
       ! "${adb[@]}" shell run-as "$package_id" \
            test -e files/rootfs/usr/lib/firefox-esr/libnssckbi.so; then
        "${adb[@]}" shell run-as "$package_id" ln -s \
            ../aarch64-linux-gnu/libnssckbi.so \
            files/rootfs/usr/lib/firefox-esr/libnssckbi.so
    fi
    # Profile id is firefox-esr; seed a first-run-skipping user.js.
    "${adb[@]}" shell run-as "$package_id" mkdir -p \
        files/homes/firefox-esr/online
    tmp="/data/local/tmp/bionicx-firefox-online-user.js"
    "${adb[@]}" push \
        "$bundle_dir/app/fixtures/firefox-online-user.js" "$tmp" >/dev/null
    "${adb[@]}" shell run-as "$package_id" cp "$tmp" \
        files/homes/firefox-esr/online/user.js
    "${adb[@]}" shell rm "$tmp"
fi

screenshot="${BIONICX_SCREENSHOT:-$repo_dir/build/${profile_name}-device.png}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop "$package_id"
"${adb[@]}" shell "run-as $package_id sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
sleep 0.3
"${adb[@]}" shell am start -W \
    -n "$package_id/com.winlator.BionicXActivity"
"${adb[@]}" shell cmd statusbar collapse >/dev/null 2>&1 || true

# Hold the window long enough for compositor pixels, then keep the largest shot.
best_bytes=0
for _ in $(seq 1 40); do
    "${adb[@]}" exec-out screencap -p > "$screenshot"
    bytes="$(wc -c < "$screenshot" | tr -d ' ')"
    if [[ "$bytes" -gt "$best_bytes" ]]; then
        best_bytes="$bytes"
        cp "$screenshot" "${screenshot}.best"
    fi
    if [[ "$bytes" -gt 40000 ]]; then
        break
    fi
    sleep 1
done
if [[ -f "${screenshot}.best" ]]; then
    mv "${screenshot}.best" "$screenshot"
fi
echo "screenshot $screenshot bytes=$(wc -c < "$screenshot" | tr -d ' ')"
"${adb[@]}" logcat -d -v brief | grep -E \
    'BionicX|launching|running untraced|exited|FATAL|fatal|Error:' | tail -n 80
