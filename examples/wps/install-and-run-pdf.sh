#!/usr/bin/env bash
# Open the controlled two-page fixture in untraced wpspdf after libtiff5.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
screenshot="${BIONICX_SCREENSHOT:-$repo_dir/build/evidence/wps-pdf-live.png}"
mkdir -p "$(dirname "$screenshot")"

ANDROID_SERIAL="$serial" \
    "$repo_dir/examples/wps-pdf-tiff-probe/install-and-run.sh"

app_root="$repo_dir/build/wps-pdf-app"
mkdir -p "$app_root/fixtures"
python3 "$repo_dir/examples/wps/build-pdf-fixture.py" \
    "$app_root/fixtures/BionicX-PDF-Integration.pdf"

"$repo_dir/tools/install-profile.sh" \
    --serial "$serial" \
    --profile "$repo_dir/profiles/wps-pdf.json" \
    --app-root "$app_root"

# Fresh profile homes re-show the Kingsoft EULA. Seed the same accepted
# flag Writer already uses so the PDF page is what we screenshot.
tmp_conf="/data/local/tmp/wps-accepted-eula-$$.conf"
"${adb[@]}" push "$repo_dir/examples/wps/accepted-eula.conf" "$tmp_conf" \
    >/dev/null
"${adb[@]}" shell run-as io.taowen.bx mkdir -p \
    files/homes/wps-pdf/.config/Kingsoft
"${adb[@]}" shell run-as io.taowen.bx cp "$tmp_conf" \
    files/homes/wps-pdf/.config/Kingsoft/Office.conf
"${adb[@]}" shell rm -f "$tmp_conf"

"${adb[@]}" shell 'kill -9 $(pidof cupsd) $(pidof bionicx-exec) 2>/dev/null; true'
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null

wait_log() {
    local pattern="$1"
    for _ in $(seq 1 400); do
        if "${adb[@]}" logcat -d -v brief | grep -Fq "$pattern"; then
            return 0
        fi
        sleep 0.1
    done
    echo "timed out waiting for device log: $pattern" >&2
    return 1
}

wait_log "bionicx-exec: running untraced"
# First frames are empty chrome or the EULA box. Give Qt time to rasterize
# the fixture before polling for ink on a light page.
sleep 8
mkdir -p "$(dirname "$screenshot")"
ok=0
for _ in $(seq 1 40); do
    sleep 1
    "${adb[@]}" exec-out screencap -p > "$screenshot"
    if python3 "$repo_dir/examples/wps/assert-live-pdf.py" "$screenshot"; then
        ok=1
        break
    fi
done
[[ "$ok" -eq 1 ]] || {
    echo "WPS PDF screenshot stayed black: $screenshot" >&2
    "${adb[@]}" logcat -d -v brief | grep -E 'wpspdf|libtiff|exited' || true
    exit 1
}
if "${adb[@]}" logcat -d -v brief | grep -E 'libtiff.so.5: cannot open|exited with 255'; then
    echo "wpspdf still missing libtiff.so.5" >&2
    exit 1
fi
echo "untraced wpspdf live page: PASS"
