#!/usr/bin/env bash
# Install / reinstall / remove on the shared seed. Every declared package
# must stay in one dpkg database with no per-app system libraries.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
bxapt=("$repo_dir/tools/bxapt" --serial "$serial")
evidence="${BIONICX_EVIDENCE:-$repo_dir/build/evidence/dpkg-consistency}"
mkdir -p "$evidence"
log="$evidence/dpkg-consistency.log"
exec > >(tee "$log") 2>&1

strip_exec() {
    { grep -v -e '^bionicx-exec:' -e ' file pushed,' -e 'skipped\.' || true; } \
        | tr -d '\r'
}

declared_packages() {
    local line package extra
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%%#*}"
        read -r package extra <<< "$line"
        [[ -n "${package:-}" ]] || continue
        printf '%s\n' "$package"
    done < "$repo_dir/packages/trixie-popular.txt"
    while IFS=$'\t' read -r package _rest || [[ -n "${package:-}" ]]; do
        [[ -n "${package:-}" && "$package" != \#* ]] || continue
        printf '%s\n' "$package"
    done < "$repo_dir/packages/external-arm64.tsv"
    printf '%s\n' cups-daemon cups-client
}

mapfile -t declared < <(declared_packages | awk 'NF && !seen[$0]++')
echo "BXINFO declared ${#declared[@]} packages: ${declared[*]}"

echo "BXINFO seed"
"${adb[@]}" shell run-as "$package_id" cat files/rootfs/.bionicx-rootfs-seed-id

must_empty_audit() {
    local name="$1"
    local audit
    audit="$("${bxapt[@]}" dpkg --audit 2>&1 | tee "$evidence/$name" | strip_exec)"
    if [[ -n "${audit//[$'\t\n ']}" ]]; then
        echo "BXTEST FAIL audit-$name"
        printf '%s\n' "$audit"
        exit 1
    fi
    echo "BXTEST PASS audit-$name empty"
}

must_all_ii() {
    local name="$1"
    shift
    local pkg status
    local report="$evidence/$name"
    : > "$report"
    for pkg in "$@"; do
        status="$("${bxapt[@]}" query -W -f='${Package} ${Status} ${Version}\n' \
            "$pkg" 2>&1 | strip_exec | grep -F "$pkg" | tail -n 1 || true)"
        printf '%s\n' "$status" | tee -a "$report"
        if ! grep -Fq 'install ok installed' <<<"$status"; then
            echo "BXTEST FAIL $name $pkg not-ii"
            printf '%s\n' "$status"
            exit 1
        fi
    done
    echo "BXTEST PASS $name all-ii count=$#"
}

must_absent() {
    local pkg="$1"
    local status
    status="$("${bxapt[@]}" query -W -f='${db:Status-Abbrev}\n' "$pkg" 2>&1 \
        | strip_exec || true)"
    if grep -Eq '^ii' <<<"$status"; then
        echo "BXTEST FAIL $pkg still-installed"
        printf '%s\n' "$status"
        exit 1
    fi
    echo "BXTEST PASS $pkg absent"
}

must_no_per_app_system_libs() {
    local found
    found="$(
        {
            "${adb[@]}" shell run-as "$package_id" find files/apps -name libc.so.6
            "${adb[@]}" shell run-as "$package_id" find files/apps -name ld-linux-aarch64.so.1
            "${adb[@]}" shell run-as "$package_id" find files/apps -name 'libstdc++.so.6'
        } | tr -d '\r'
    )"
    if [[ -n "${found//[$'\t\n ']}" ]]; then
        echo "BXTEST FAIL per-app-system-libs"
        printf '%s\n' "$found"
        exit 1
    fi
    echo "BXTEST PASS per-app-system-libs none"
}

must_empty_audit dpkg-audit-before.log
must_all_ii dpkg-declared-before.log "${declared[@]}"
must_no_per_app_system_libs

echo "==== reinstall declared leaf bsdextrautils ===="
"${bxapt[@]}" install --reinstall bsdextrautils
must_empty_audit dpkg-audit-after-reinstall.log
must_all_ii dpkg-declared-after-reinstall.log "${declared[@]}"
must_no_per_app_system_libs

echo "==== remove declared ristretto ===="
"${bxapt[@]}" remove ristretto
must_empty_audit dpkg-audit-after-remove.log
must_absent ristretto
others=()
for pkg in "${declared[@]}"; do
    [[ "$pkg" == ristretto ]] && continue
    others+=("$pkg")
done
must_all_ii dpkg-declared-after-remove.log "${others[@]}"
must_no_per_app_system_libs

echo "==== bxapt set restores ristretto ===="
"${bxapt[@]}" set "$repo_dir/packages/trixie-popular.txt"
must_empty_audit dpkg-audit-after-set.log
must_all_ii dpkg-declared-after-set.log "${declared[@]}"
must_no_per_app_system_libs

echo "BXSUMMARY dpkg-consistency install/reinstall/remove all-declared"
