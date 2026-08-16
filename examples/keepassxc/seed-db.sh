#!/usr/bin/env bash
# Seed the KeePassXC app fixture via keepassxc-cli. Not a protocol probe:
# the DatabaseWidget NULL d_ptr repro is keepassxc-db-widget-probe.
# App-only: the fixture must not replace the device seed.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
files="/data/user/0/$package_id/files"
root="$files/rootfs"
fixture="$files/apps/keepassxc/fixtures"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")

cli() {
    "${adb[@]}" shell run-as "$package_id" \
        "$files/bin/bionicx-exec" --cwd "$root" \
        --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
        --env "BIONICX_ROOTFS=$root" \
        --env "BIONICX_TMPDIR=$files/run/bxapt" \
        --env "HOME=$files/homes/keepassxc" \
        --env "PATH=$root/usr/bin:$root/bin" \
        --env "QT_QPA_PLATFORM=offscreen" \
        --env "QT_PLUGIN_PATH=$root/usr/lib/aarch64-linux-gnu/qt5/plugins" \
        --env "LANG=C.UTF-8" \
        -- "$root/usr/bin/keepassxc-cli" "$@"
}

passed=0
failed=0
check() {
    local name="$1" ok="$2" detail="${3:-}"
    if [[ "$ok" == 1 ]]; then
        printf 'BXTEST PASS %s%s\n' "$name" "${detail:+ $detail}"
        passed=$((passed + 1))
    else
        printf 'BXTEST FAIL %s%s\n' "$name" "${detail:+ $detail}"
        failed=$((failed + 1))
    fi
}

"${adb[@]}" shell run-as "$package_id" mkdir -p \
    files/apps/keepassxc/bin files/apps/keepassxc/fixtures files/homes/keepassxc
tmp_open="/data/local/tmp/bionicx-keepassxc-deferred-open"
"${adb[@]}" push "$repo_dir/examples/keepassxc/keepassxc-deferred-open" \
    "$tmp_open" >/dev/null
"${adb[@]}" shell chmod 644 "$tmp_open"
"${adb[@]}" shell run-as "$package_id" cp "$tmp_open" \
    files/apps/keepassxc/bin/keepassxc-deferred-open
"${adb[@]}" shell run-as "$package_id" chmod 755 \
    files/apps/keepassxc/bin/keepassxc-deferred-open
"${adb[@]}" shell rm "$tmp_open"
"${adb[@]}" shell run-as "$package_id" rm -f \
    files/apps/keepassxc/fixtures/bionicx.kdbx
tmp="/data/local/tmp/bionicx-keepassxc.key"
"${adb[@]}" push "$repo_dir/examples/keepassxc/fixtures/bionicx.key" \
    "$tmp" >/dev/null
"${adb[@]}" shell chmod 644 "$tmp"
"${adb[@]}" shell run-as "$package_id" cp "$tmp" \
    files/apps/keepassxc/fixtures/bionicx.key
"${adb[@]}" shell rm "$tmp"

create_out="$(cli db-create --set-key-file "$fixture/bionicx.key" \
    "$fixture/bionicx.kdbx" 2>&1 || true)"
printf '%s\n' "$create_out"
if grep -Fq 'Successfully created new database.' <<<"$create_out"; then
    check db-create 1 created
else
    check db-create 0 "$create_out"
fi

add_out="$(cli add --key-file "$fixture/bionicx.key" --no-password \
    -u bionicx --url https://example.com --notes BionicX-keepassxc \
    --generate -L 20 "$fixture/bionicx.kdbx" login 2>&1 || true)"
printf '%s\n' "$add_out"
if grep -Fq 'Successfully added entry login.' <<<"$add_out"; then
    check db-add 1 login
else
    check db-add 0 "$add_out"
fi

ls_out="$(cli ls --key-file "$fixture/bionicx.key" --no-password \
    "$fixture/bionicx.kdbx" 2>&1 || true)"
printf '%s\n' "$ls_out"
if grep -Fxq login <<<"$ls_out"; then
    check db-ls 1 login
else
    check db-ls 0 "$ls_out"
fi

show_out="$(cli show --key-file "$fixture/bionicx.key" --no-password \
    -a Title -a UserName -a URL "$fixture/bionicx.kdbx" login 2>&1 || true)"
printf '%s\n' "$show_out"
if grep -Fxq login <<<"$show_out" &&
   grep -Fxq bionicx <<<"$show_out" &&
   grep -Fxq https://example.com <<<"$show_out"; then
    check db-show 1 'login bionicx example.com'
else
    check db-show 0 "$show_out"
fi

reopen_out="$(cli ls --key-file "$fixture/bionicx.key" --no-password \
    "$fixture/bionicx.kdbx" 2>&1 || true)"
printf '%s\n' "$reopen_out"
if grep -Fxq login <<<"$reopen_out"; then
    check db-reopen 1 login
else
    check db-reopen 0 "$reopen_out"
fi

size="$("${adb[@]}" shell run-as "$package_id" \
    stat -c %s files/apps/keepassxc/fixtures/bionicx.kdbx | tr -d '\r')"
if [[ "$size" =~ ^[0-9]+$ ]] && [[ "$size" -gt 0 ]]; then
    check db-persist 1 "bytes=$size"
else
    check db-persist 0 "bytes=$size"
fi

summary="$(printf 'BXSUMMARY keepassxc-cli passed=%s failed=%s' "$passed" "$failed")"
printf '%s\n' "$summary"
grep -Fq "BXSUMMARY keepassxc-cli passed=6 failed=0" <<<"$summary"
