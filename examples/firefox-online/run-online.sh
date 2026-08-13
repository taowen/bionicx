#!/usr/bin/env bash
# Untraced Firefox ESR online navigation plus a force-stop cold restart.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
evidence="${BIONICX_EVIDENCE:-$repo_dir/evidence/rebuild-2026-08-14}"
mkdir -p "$evidence"

log="$evidence/firefox-esr-online.log"
exec > >(tee "$log") 2>&1

echo "BXINFO seed"
"${adb[@]}" shell run-as "$package_id" cat files/rootfs/.bionicx-rootfs-seed-id

if [[ -z "${BIONICX_DNS_SERVERS:-}" ]]; then
    BIONICX_DNS_SERVERS="$("${adb[@]}" shell getprop net.dns1 2>/dev/null | tr -d '\r')"
fi
if [[ -z "${BIONICX_DNS_SERVERS:-}" || "$BIONICX_DNS_SERVERS" == *:* ]]; then
    BIONICX_DNS_SERVERS=8.8.8.8
fi
export BIONICX_DNS_SERVERS
echo "BXINFO dns=$BIONICX_DNS_SERVERS"
ANDROID_SERIAL="$serial" \
    "$repo_dir/examples/nss-ckbi-probe/install-and-run.sh" \
    | tee "$evidence/nss-ckbi-probe.log"
grep -E 'BXSUMMARY nss-ckbi passed=[0-9]+ failed=0' \
    "$evidence/nss-ckbi-probe.log"

ANDROID_SERIAL="$serial" \
BIONICX_SCREENSHOT="$evidence/firefox-esr-online.png" \
    "$repo_dir/examples/popular-apps/install-and-run.sh" firefox-esr-online

wait_page() {
    local shot="$1"
    local ok=0
    for _ in $(seq 1 50); do
        "${adb[@]}" exec-out screencap -p > "$shot"
        if python3 "$repo_dir/examples/firefox-online/assert-example-page.py" \
                "$shot"; then
            ok=1
            break
        fi
        sleep 2
    done
    [[ "$ok" -eq 1 ]]
}

wait_page "$evidence/firefox-esr-online.png"

profile_dir=files/homes/firefox-esr/online
"${adb[@]}" shell run-as "$package_id" test -s "$profile_dir/cert9.db"
"${adb[@]}" shell run-as "$package_id" test -s "$profile_dir/key4.db"
echo "BXTEST PASS firefox-nss-profile cert9+key4"

tmp_places="/data/local/tmp/firefox-places-$$.sqlite"
"${adb[@]}" shell run-as "$package_id" \
    cp "$profile_dir/places.sqlite" "$tmp_places" 2>/dev/null || true
if "${adb[@]}" pull "$tmp_places" "$evidence/firefox-places.sqlite" >/dev/null 2>&1; then
    "${adb[@]}" shell rm -f "$tmp_places"
    if sqlite3 "$evidence/firefox-places.sqlite" \
            "SELECT url FROM moz_places;" | grep -Fq 'example.com'; then
        echo "BXTEST PASS firefox-places example.com"
    else
        echo "BXTEST FAIL firefox-places missing example.com"
        sqlite3 "$evidence/firefox-places.sqlite" "SELECT url FROM moz_places;" || true
        exit 1
    fi
else
    echo "BXINFO firefox-places not flushed yet"
fi

echo "BXINFO cold-start"
"${adb[@]}" shell 'kill -9 $(pidof firefox-esr) $(pidof bionicx-exec) 2>/dev/null; true'
"${adb[@]}" shell am force-stop "$package_id"
"${adb[@]}" logcat -c
"${adb[@]}" shell am start -W \
    -n "$package_id/com.winlator.BionicXActivity" >/dev/null
sleep 8
wait_page "$evidence/firefox-esr-online-cold.png"
"${adb[@]}" shell run-as "$package_id" test -s "$profile_dir/cert9.db"
echo "BXTEST PASS firefox-online-cold cert9"
echo "BXSUMMARY firefox-online warm+cold"
