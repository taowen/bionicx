#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_GTK_ICON_THEME_BUNDLE:-$repo_dir/build/gtk-icon-theme-probe-bundle}"

"$repo_dir/examples/gtk-icon-theme-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/gtk-icon-theme-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
for i in $(seq 1 30); do
    if adb -s "$serial" logcat -d -v brief \
            | grep -Fq 'BXSUMMARY gtk-icon-theme'; then
        echo "gtk-icon-theme probe finished at ${i}s"
        break
    fi
    sleep 1
done
result="$(adb -s "$serial" logcat -d -v brief \
    | grep -E 'BXTEST|BXSUMMARY')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY gtk-icon-theme passed=7 failed=0" <<<"$result"
echo "gtk-icon-theme probe: PASS"
