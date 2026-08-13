#!/usr/bin/env bash
set -euo pipefail

# Install the BionicX APK without a manual package-installer tap.
#
# OriginOS on vivo X300 / V2509A (1216x2640) shows a risk page for adb/pm
# installs. Those devices get a background tap loop while `pm install -r -t`
# runs. Other devices use `adb install -r -t`.
#
# After a successful install the helper also stages files/bin/bionicx-exec and
# files/lib/libbionicx-runtime.so from the APK so `bxapt normalize` works
# before the first Activity launch.

usage() {
    echo "usage: $0 [--serial SERIAL] [--extract-only] [--dry-run] [APK...]" >&2
    echo "default APK: build/bionicx-debug.apk" >&2
}

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
serial="${ANDROID_SERIAL:-}"
extract_only=0
dry_run=0
package="io.taowen.bx"
check_x=607
check_y=2289
continue_x=607
continue_y=2462
tap_rounds="${TAP_ROUNDS:-260}"
tap_interval="${TAP_INTERVAL:-0.8}"
install_timeout="${INSTALL_TIMEOUT:-300s}"
adb_wait_timeout="${ADB_WAIT_TIMEOUT:-10s}"
remote_dir="${REMOTE_DIR:-/data/local/tmp}"

apks=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --serial)
            [[ -n "${2:-}" ]] || { usage; exit 2; }
            serial="$2"
            shift 2
            ;;
        --extract-only) extract_only=1; shift ;;
        --dry-run) dry_run=1; shift ;;
        -h|--help) usage; exit 0 ;;
        --) shift; apks+=("$@"); break ;;
        -*)
            echo "unknown argument: $1" >&2
            usage
            exit 2
            ;;
        *) apks+=("$1"); shift ;;
    esac
done
if [[ ${#apks[@]} -eq 0 ]]; then
    apks=("$repo_dir/build/bionicx-debug.apk")
fi

adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=(-s "$serial")

adb_shell() {
    "${adb[@]}" shell "$@"
}

is_vivo_originos_confirm() {
    local model product device name
    model="$(adb_shell getprop ro.product.model 2>/dev/null | tr -d '\r')"
    product="$(adb_shell getprop ro.product.product.name 2>/dev/null | tr -d '\r')"
    device="$(adb_shell getprop ro.product.device 2>/dev/null | tr -d '\r')"
    name="$(adb_shell getprop ro.product.name 2>/dev/null | tr -d '\r')"
    [[ "$model" == "V2509A" || "$product" == "V2509A" ||
        "$device" == "V2509A" || "$name" == "V2509A" ||
        "$model" == *"X300"* || "$product" == *"X300"* ||
        "$device" == *"X300"* || "$name" == *"X300"* ]]
}

wake_for_taps() {
    # OriginOS confirmation is a visual page. A dark or locked screen swallows
    # the fixed taps. KEYCODE_WAKEUP can throw on some builds; keep going.
    adb_shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true
    adb_shell svc power stayon true >/dev/null 2>&1 || true
    adb_shell wm dismiss-keyguard >/dev/null 2>&1 || true
}

tap_installer_loop() {
    local rounds="$1"
    local interval="$2"
    local i
    for ((i = 0; i < rounds; i++)); do
        adb_shell input tap "$check_x" "$check_y" >/dev/null 2>&1 || true
        sleep 0.15
        adb_shell input tap "$continue_x" "$continue_y" >/dev/null 2>&1 || true
        sleep "$interval"
    done
}

extract_executor() {
    local apk="$1"
    if ! unzip -l "$apk" 2>/dev/null | grep -q 'assets/bionicx/bin/bionicx-exec'; then
        echo "APK has no bionicx-exec asset; skip extract: $apk" >&2
        return 0
    fi
    if [[ "$dry_run" -eq 1 ]]; then
        echo "dry-run: extract bionicx-exec and libbionicx-runtime.so from $apk"
        return 0
    fi
    local staging
    staging="$(mktemp -d "${TMPDIR:-/tmp}/bionicx-apk-XXXXXX")"
    unzip -p "$apk" assets/bionicx/bin/bionicx-exec \
        > "$staging/bionicx-exec"
    unzip -p "$apk" assets/bionicx/lib/libbionicx-runtime.so \
        > "$staging/libbionicx-runtime.so"
    chmod 700 "$staging/bionicx-exec"
    local remote_exec="$remote_dir/bionicx-exec-$$"
    local remote_runtime="$remote_dir/libbionicx-runtime-$$.so"
    "${adb[@]}" push "$staging/bionicx-exec" "$remote_exec" >/dev/null
    "${adb[@]}" push "$staging/libbionicx-runtime.so" "$remote_runtime" >/dev/null
    adb_shell run-as "$package" mkdir -p files/bin files/lib
    adb_shell run-as "$package" cp "$remote_exec" files/bin/bionicx-exec
    adb_shell run-as "$package" cp "$remote_runtime" files/lib/libbionicx-runtime.so
    adb_shell run-as "$package" chmod 700 files/bin/bionicx-exec
    adb_shell rm -f "$remote_exec" "$remote_runtime" >/dev/null 2>&1 || true
    rm -rf "$staging"
    echo "staged $package files/bin/bionicx-exec"
}

install_one() {
    local apk="$1"
    if [[ ! -f "$apk" ]]; then
        echo "missing apk: $apk" >&2
        return 2
    fi
    if [[ "$extract_only" -eq 1 ]]; then
        extract_executor "$apk"
        return 0
    fi

    if [[ "$dry_run" -eq 1 ]]; then
        if is_vivo_originos_confirm; then
            echo "dry-run: V2509A/X300 OriginOS confirm; pm install -r -t + tap ($check_x,$check_y) ($continue_x,$continue_y): $apk"
        else
            echo "dry-run: adb install -r -t $apk"
        fi
        extract_executor "$apk"
        return 0
    fi

    timeout "$adb_wait_timeout" "${adb[@]}" wait-for-device

    if is_vivo_originos_confirm; then
        echo "detected V2509A/X300 ($serial); using OriginOS installer taps"
        wake_for_taps
        local remote_apk="$remote_dir/$(basename "$apk")"
        "${adb[@]}" push "$apk" "$remote_apk"
        tap_installer_loop "$tap_rounds" "$tap_interval" &
        local tap_pid=$!
        local install_rc=0
        set +e
        timeout "$install_timeout" "${adb[@]}" shell pm install -r -t "$remote_apk"
        install_rc=$?
        set -e
        kill "$tap_pid" >/dev/null 2>&1 || true
        wait "$tap_pid" >/dev/null 2>&1 || true
        adb_shell rm -f "$remote_apk" >/dev/null 2>&1 || true
        if [[ "$install_rc" -ne 0 ]]; then
            echo "pm install failed or timed out for $apk (exit=$install_rc)" >&2
            return "$install_rc"
        fi
    else
        "${adb[@]}" install -r -t "$apk"
    fi
    extract_executor "$apk"
}

for apk in "${apks[@]}"; do
    install_one "$apk"
done
echo "installed ${#apks[@]} apk(s)"
