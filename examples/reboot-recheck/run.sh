#!/usr/bin/env bash
# Force-stop + device reboot, then rerun host probes, GPU/GLX, CLI and
# one untraced real-app profile. Diagnostics stay off for the apps.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
evidence="${BIONICX_EVIDENCE:-$repo_dir/build/evidence/reboot-recheck}"
mkdir -p "$evidence"
log="$evidence/reboot-recheck.log"
exec > >(tee "$log") 2>&1

expected_seed=ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2
payload_host="$repo_dir/build/popular-apps-bundle/app/fixtures/bionicx-network-payload.bin"
if [[ ! -f "$payload_host" ]]; then
    "$repo_dir/examples/popular-apps/build-bundle.sh" \
        "$repo_dir/build/popular-apps-bundle" >/dev/null
fi
expected_payload="$(sha256sum "$payload_host" | cut -d' ' -f1)"

echo "==== host probes ===="
"$repo_dir/tests/test-runtime-contract.sh"
"$repo_dir/tests/test-new-device-guide.sh"
"$repo_dir/tests/test-glx-probe-seed-safe.sh"
"$repo_dir/tests/test-krita-glx-destroy-probe.sh"
"$repo_dir/tests/test-keepassxc-seed.sh"
"$repo_dir/tests/test-popular-durable-workflows.sh"
"$repo_dir/tests/test-dpkg-consistency.sh"
"$repo_dir/tests/test-firefox-online.sh"

seed_now() {
    "${adb[@]}" shell run-as "$package_id" \
        cat files/rootfs/.bionicx-rootfs-seed-id | tr -d '\r'
}

payload_now() {
    "${adb[@]}" exec-out run-as "$package_id" \
        sha256sum files/homes/qbittorrent/Downloads/bionicx-network-payload.bin \
        | awk '{print $1}'
}

echo "==== pre-reboot state ===="
seed="$(seed_now)"
echo "BXINFO seed=$seed"
[[ "$seed" == "$expected_seed" ]]
echo "BXTEST PASS seed-before $seed"
payload="$(payload_now)"
echo "BXINFO payload=$payload"
[[ "$payload" == "$expected_payload" ]]
echo "BXTEST PASS qbit-payload-before $payload"
"${adb[@]}" shell run-as "$package_id" \
    test -s files/homes/krita/Documents/bionicx-saved.png
echo "BXTEST PASS krita-export-before"
"${adb[@]}" shell run-as "$package_id" \
    test -s files/apps/keepassxc/fixtures/bionicx.kdbx
echo "BXTEST PASS keepassxc-kdbx-before"

echo "==== force-stop + reboot ===="
"${adb[@]}" shell am force-stop "$package_id"
echo "BXINFO force-stop done"
"${adb[@]}" reboot
echo "BXINFO reboot issued"
"${adb[@]}" wait-for-device
"${adb[@]}" shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true
"${adb[@]}" shell svc power stayon true >/dev/null 2>&1 || true
"${adb[@]}" shell settings put system screen_off_timeout 1800000 >/dev/null 2>&1 || true
"${adb[@]}" shell wm dismiss-keyguard >/dev/null 2>&1 || true
"${adb[@]}" shell input swipe 960 1000 960 200 400 >/dev/null 2>&1 || true
ready=0
for _ in $(seq 1 90); do
    if "${adb[@]}" shell run-as "$package_id" \
            cat files/rootfs/.bionicx-rootfs-seed-id >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 2
done
[[ "$ready" -eq 1 ]]
"${adb[@]}" shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true
"${adb[@]}" shell svc power stayon true >/dev/null 2>&1 || true
"${adb[@]}" shell settings put system screen_off_timeout 1800000 >/dev/null 2>&1 || true
"${adb[@]}" shell wm dismiss-keyguard >/dev/null 2>&1 || true
"${adb[@]}" shell input swipe 960 1000 960 200 400 >/dev/null 2>&1 || true
# Package manager can still reject run-as push for a few seconds.
sleep 5
echo "BXTEST PASS run-as-after-reboot"

seed="$(seed_now)"
echo "BXINFO seed-after=$seed"
[[ "$seed" == "$expected_seed" ]]
echo "BXTEST PASS seed-after $seed"
payload="$(payload_now)"
[[ "$payload" == "$expected_payload" ]]
echo "BXTEST PASS qbit-payload-after $payload"
"${adb[@]}" shell run-as "$package_id" \
    test -s files/homes/krita/Documents/bionicx-saved.png
echo "BXTEST PASS krita-export-after"
audit=""
for _ in $(seq 1 10); do
    audit="$("$repo_dir/tools/bxapt" --serial "$serial" dpkg --audit 2>&1 \
        | grep -v -e '^bionicx-exec:' -e ' file pushed,' -e 'skipped\.' \
        -e 'packagelist_parse' || true)"
    if [[ -z "${audit//[$'\t\n\r ']}" ]]; then
        break
    fi
    sleep 3
done
if [[ -n "${audit//[$'\t\n\r ']}" ]]; then
    echo "BXTEST FAIL audit-after-reboot"
    printf '%s\n' "$audit"
    exit 1
fi
echo "BXTEST PASS audit-after-reboot empty"

echo "==== device probes after reboot ===="
ANDROID_SERIAL="$serial" \
    "$repo_dir/examples/keepassxc/seed-db.sh" \
    | tee "$evidence/keepassxc-seed-reboot.log"
grep -Fq "BXSUMMARY keepassxc-cli passed=6 failed=0" \
    "$evidence/keepassxc-seed-reboot.log"

ANDROID_SERIAL="$serial" \
BIONICX_SCREENSHOT="$evidence/glx-probe-reboot.png" \
    "$repo_dir/examples/glx-probe/install-and-run.sh" \
    | tee "$evidence/glx-probe-reboot.log"
grep -Fq "BXSUMMARY host-glx passed=5 failed=0" \
    "$evidence/glx-probe-reboot.log"

echo "==== untraced desktop session after reboot ===="
ANDROID_SERIAL="$serial" \
BIONICX_DESKTOP_SCREENSHOT="$evidence/desktop-session-reboot.png" \
BIONICX_DESKTOP_MAPPED="$evidence/desktop-session-reboot-mapped.png" \
    "$repo_dir/examples/desktop-session/install-and-run.sh" \
    | tee "$evidence/desktop-session-reboot.log"
grep -Fq "BXSUMMARY desktop-session-accept passed=7 failed=0" \
    "$evidence/desktop-session-reboot.log"

echo "BXSUMMARY reboot-recheck seed+audit+glx5+keepassxc6+desktop7"
