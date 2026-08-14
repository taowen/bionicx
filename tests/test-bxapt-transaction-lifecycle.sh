#!/usr/bin/env bash
# Device acceptance for the P0 package-transaction lifecycle.
# Requires ANDROID_SERIAL and a correctly installed seed at files/rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
evidence_dir="$repo_dir/build/evidence/bxapt-transaction"
mkdir -p "$evidence_dir"
bxapt=("$repo_dir/tools/bxapt" --serial "$serial")
adb=("${ADB:-adb}" -s "$serial")

requested_packages=(uuid-runtime)
account_user=uuidd
account_group=uuidd

run_as() {
    local command="run-as $package_id"
    local argument
    for argument in "$@"; do
        command+=" $(printf "'%s'" "${argument//\'/\'\\\'\'}")"
    done
    "${adb[@]}" shell "$command"
}

strip_exec() {
    { grep -v -e '^bionicx-exec:' -e ' file pushed,' -e 'skipped\.' || true; } \
        | tr -d '\r'
}

capture() {
    local name="$1"
    shift
    "$@" | tee "$evidence_dir/$name"
}

must_empty_audit() {
    local name="$1"
    local audit
    audit="$("${bxapt[@]}" dpkg --audit 2>&1 | tee "$evidence_dir/$name" | strip_exec)"
    [[ -z "${audit//[$'\t\n ']}" ]]
}

must_status() {
    local pkg="$1"
    local expected="$2"
    local name="$3"
    local status
    status="$("${bxapt[@]}" query -W -f='${db:Status-Abbrev}\n' "$pkg" \
        2>&1 | tee "$evidence_dir/$name" | strip_exec | awk '{ print $1 }')"
    [[ "$status" == "$expected" ]]
}

must_absent() {
    local pkg="$1"
    local name="$2"
    local status
    status="$("${bxapt[@]}" query -W -f='${db:Status-Abbrev}\n' "$pkg" \
        2>&1 | tee "$evidence_dir/$name" | strip_exec | awk '{ print $1 }' || true)"
    case "$status" in
        ii|iU|iF|iH|iW|it)
            echo "package still configured: $pkg status=$status" >&2
            return 1
            ;;
    esac
}

account_line() {
    local database="$1"
    local name="$2"
    "${bxapt[@]}" run getent "$database" "$name" 2>/dev/null | tr -d '\r' || true
}

must_account() {
    local database="$1"
    local name="$2"
    local evidence="$3"
    local line
    line="$(account_line "$database" "$name" | tee "$evidence_dir/$evidence" | strip_exec)"
    [[ -n "$line" ]]
}

ledger_file=files/rootfs/var/lib/bionicx/elf-fixups.tsv

must_pruned_ledger() {
    local name="$1"
    local count
    run_as test -s "$ledger_file"
    count="$(run_as wc -l "$ledger_file" | awk '{ print $1 }' | tr -d '\r')"
    printf '%s\n' "$count" | tee "$evidence_dir/$name-count.txt"
    [[ "$count" -gt 0 ]]
}

must_marks() {
    local evidence="$1"
    shift
    local manual auto
    manual="$("${bxapt[@]}" run apt-mark showmanual | strip_exec | sort)"
    auto="$("${bxapt[@]}" run apt-mark showauto | strip_exec | sort)"
    {
        printf 'manual:\n%s\n' "$manual"
        printf 'auto:\n%s\n' "$auto"
    } | tee "$evidence_dir/$evidence"
    local package
    for package in "$@"; do
        printf '%s\n' "$manual" | grep -Fx "$package" >/dev/null
    done
}

apt_mark_has() {
    local list="$1"
    local package="$2"
    printf '%s\n' "$list" | grep -Fx "$package" >/dev/null
}

echo "==== seed layout ===="
run_as test -x files/rootfs/usr/bin/apt-get
run_as test -r files/rootfs/.bionicx-rootfs-seed-id
run_as test ! -d files/rootfs/rootfs/usr
capture lifecycle-seed-id.txt \
    run_as cat files/rootfs/.bionicx-rootfs-seed-id

echo "==== update ===="
capture lifecycle-update.log "${bxapt[@]}" update

echo "==== clean install ${requested_packages[*]} ===="
capture lifecycle-install.log "${bxapt[@]}" install "${requested_packages[@]}"
must_empty_audit lifecycle-install-audit.log
must_status uuid-runtime ii lifecycle-install-status.txt
must_account passwd "$account_user" lifecycle-install-passwd.txt
must_account group "$account_group" lifecycle-install-group.txt
must_marks lifecycle-install-marks.txt uuid-runtime
must_pruned_ledger lifecycle-install-ledger

echo "==== remove after clean install ===="
capture lifecycle-remove-first.log "${bxapt[@]}" remove uuid-runtime
must_empty_audit lifecycle-remove-first-audit.log
must_absent uuid-runtime lifecycle-remove-first-status.txt
must_pruned_ledger lifecycle-remove-first-ledger

echo "==== failed configure ===="
BIONICX_BXAPT_STOP_AFTER=unpack \
    capture lifecycle-unpack.log "${bxapt[@]}" install uuid-runtime
run_as test -s files/run/bxapt/installed-packages-before.txt
run_as test -s files/run/bxapt/requested-packages.txt
run_as test -r files/run/bxapt/unpacked-paths.txt
postinst=files/rootfs/var/lib/dpkg/info/uuid-runtime.postinst
run_as test -x "$postinst"
run_as cp "$postinst" files/run/bxapt/uuid-runtime.postinst.saved
run_as sh -c "printf '%s\n' '#!/bin/sh' 'exit 1' > $postinst"
run_as chmod 0755 "$postinst"
if "${bxapt[@]}" recover | tee "$evidence_dir/lifecycle-failed-configure.log"; then
    echo "recover succeeded with a broken postinst" >&2
    exit 1
fi
failed_status="$("${bxapt[@]}" query -W -f='${db:Status-Abbrev}\n' uuid-runtime \
    | tee "$evidence_dir/lifecycle-failed-status.txt" | strip_exec \
    | awk '{ print $1 }')"
[[ "$failed_status" == iF ]]
run_as test -s files/run/bxapt/installed-packages-before.txt

echo "==== recover ===="
run_as cp files/run/bxapt/uuid-runtime.postinst.saved "$postinst"
run_as chmod 0755 "$postinst"
capture lifecycle-recover.log "${bxapt[@]}" recover
must_empty_audit lifecycle-recover-audit.log
must_status uuid-runtime ii lifecycle-recover-status.txt
must_account passwd "$account_user" lifecycle-recover-passwd.txt
must_account group "$account_group" lifecycle-recover-group.txt
must_marks lifecycle-recover-marks.txt uuid-runtime
must_pruned_ledger lifecycle-recover-ledger
if run_as test -s files/run/bxapt/installed-packages-before.txt; then
    echo "recovery metadata was retained after success" >&2
    exit 1
fi

echo "==== remove recovered package ===="
capture lifecycle-remove.log "${bxapt[@]}" remove uuid-runtime
must_empty_audit lifecycle-remove-audit.log
must_absent uuid-runtime lifecycle-remove-status.txt
must_pruned_ledger lifecycle-remove-ledger

echo "==== autoremove ===="
capture lifecycle-autoremove.log "${bxapt[@]}" autoremove
must_empty_audit lifecycle-autoremove-audit.log
must_pruned_ledger lifecycle-autoremove-ledger
manual="$("${bxapt[@]}" run apt-mark showmanual | strip_exec)"
auto="$("${bxapt[@]}" run apt-mark showauto | strip_exec)"
{
    printf 'manual:\n%s\n' "$manual"
    printf 'auto:\n%s\n' "$auto"
} | tee "$evidence_dir/lifecycle-autoremove-marks.txt"
if apt_mark_has "$manual" uuid-runtime || apt_mark_has "$auto" uuid-runtime; then
    echo "uuid-runtime marks survived autoremove" >&2
    exit 1
fi

echo "bxapt transaction lifecycle: PASS"
