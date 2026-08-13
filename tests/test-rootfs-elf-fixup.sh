#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d "$repo_dir/build/test-elf-fixup.XXXXXXXX")"
trap 'rm -rf -- "$test_dir"' EXIT
root="$test_dir/root"
alias_root="$test_dir/alias"
mkdir -p "$root/usr/bin" "$root/usr/lib" "$root/var/lib" \
    "$alias_root/usr/lib/kept"
printf '#!/bin/sh\nexit 0\n' > "$root/usr/bin/script"
chmod 0755 "$root/usr/bin/script"

cp "$repo_dir/build/test-runtime-contract/runtime-contract-probe" \
    "$root/usr/bin/probe"
cp "$(readlink -f /lib64/ld-linux-x86-64.so.2)" \
    "$root/usr/lib/ld-linux-aarch64.so.1"
loader_before=$(sha256sum "$root/usr/lib/ld-linux-aarch64.so.1")
patchelf --set-interpreter /lib64/ld-linux-test.so.2 --force-rpath \
    --set-rpath "$root$alias_root/usr/lib/recovered:/usr/lib/redirected:\$ORIGIN/lib" \
    "$root/usr/bin/probe"

BIONICX_PATCHELF=patchelf BIONICX_READELF=readelf \
BIONICX_ROOT_ALIAS="$alias_root" \
    "$repo_dir/tools/rootfs-elf-fixup.sh" "$root" >/dev/null

[[ "$(patchelf --print-interpreter "$root/usr/bin/probe")" == \
    "$root/usr/lib/ld-linux-aarch64.so.1" ]]
system_runpath="$root/usr/lib:$root/usr/lib/aarch64-linux-gnu:$root/lib:$root/lib/aarch64-linux-gnu:\$ORIGIN:\$ORIGIN/../lib"
[[ "$(patchelf --print-rpath "$root/usr/bin/probe")" == \
    "$system_runpath:$alias_root/usr/lib/recovered:$root/usr/lib/redirected:\$ORIGIN/lib" ]]
grep -F $'/usr/bin/probe\tPT_INTERP\t/lib64/ld-linux-test.so.2\t'"$root/usr/lib/ld-linux-aarch64.so.1" \
    "$root/var/lib/bionicx/elf-fixups.tsv" >/dev/null
grep -F $'/usr/bin/probe\tRUNPATH\t' \
    "$root/var/lib/bionicx/elf-fixups.tsv" >/dev/null
grep -F $'/usr/bin/probe\tDT_RPATH\tDT_RUNPATH' \
    "$root/var/lib/bionicx/elf-fixups.tsv" >/dev/null
readelf -d "$root/usr/bin/probe" | grep -q '(RUNPATH)'
cmp -s "$root/usr/bin/script" <(printf '#!/bin/sh\nexit 0\n')
[[ "$(sha256sum "$root/usr/lib/ld-linux-aarch64.so.1")" == "$loader_before" ]]

mkdir -p "$root/opt/vendor/qt/plugins/platforms"
cp "$repo_dir/build/test-runtime-contract/runtime-contract-probe" \
    "$root/opt/vendor/qt/plugins/platforms/direct-plugin.so"
cp "$repo_dir/build/test-runtime-contract/runtime-contract-probe" \
    "$root/opt/vendor/libbionicx-direct.so.1"
cp "$repo_dir/build/test-runtime-contract/runtime-contract-probe" \
    "$root/opt/untouched-probe"
patchelf --set-interpreter /lib64/ld-linux-untouched.so.2 \
    "$root/opt/untouched-probe"
patchelf --add-needed libbionicx-direct.so.1 \
    "$root/opt/vendor/qt/plugins/platforms/direct-plugin.so"
manifest="$test_dir/unpacked-paths.txt"
printf '%s\n' \
    './opt/vendor/qt/plugins/platforms/direct-plugin.so' \
    './opt/vendor/libbionicx-direct.so.1' > "$manifest"
BIONICX_PATCHELF=patchelf BIONICX_READELF=readelf \
    "$repo_dir/tools/rootfs-elf-fixup.sh" --paths-from "$manifest" \
        "$root" >/dev/null
case "$(patchelf --print-rpath "$root/opt/vendor/qt/plugins/platforms/direct-plugin.so")" in
    *"$root/opt/vendor"*) ;;
    *) echo "unique direct dependency directory was not normalized" >&2; exit 1 ;;
esac
grep -F $'/opt/vendor/qt/plugins/platforms/direct-plugin.so\tDT_NEEDED\tlibbionicx-direct.so.1\t'"$root/opt/vendor" \
    "$root/var/lib/bionicx/elf-fixups.tsv" >/dev/null
grep -F $'/usr/bin/probe\tPT_INTERP\t' \
    "$root/var/lib/bionicx/elf-fixups.tsv" >/dev/null
[[ "$(patchelf --print-interpreter "$root/opt/untouched-probe")" == \
    /lib64/ld-linux-untouched.so.2 ]]
ledger_before_empty=$(sha256sum "$root/var/lib/bionicx/elf-fixups.tsv")
: > "$manifest"
BIONICX_PATCHELF=patchelf BIONICX_READELF=readelf \
    "$repo_dir/tools/rootfs-elf-fixup.sh" --paths-from "$manifest" \
        "$root" >/dev/null
[[ "$(sha256sum "$root/var/lib/bionicx/elf-fixups.tsv")" == \
    "$ledger_before_empty" ]]
unlink "$root/opt/vendor/qt/plugins/platforms/direct-plugin.so"
BIONICX_PATCHELF=patchelf BIONICX_READELF=readelf \
    "$repo_dir/tools/rootfs-elf-fixup.sh" --paths-from "$manifest" \
        "$root" >/dev/null
if grep -F '/opt/vendor/qt/plugins/platforms/direct-plugin.so' \
        "$root/var/lib/bionicx/elf-fixups.tsv" >/dev/null; then
    echo "removed ELF survived incremental ledger pruning" >&2
    exit 1
fi
grep -F $'/usr/bin/probe\tPT_INTERP\t' \
    "$root/var/lib/bionicx/elf-fixups.tsv" >/dev/null

app="$test_dir/app"
mkdir -p "$app/bin"
cp "$repo_dir/build/test-runtime-contract/runtime-contract-probe" \
    "$app/bin/app-probe"
patchelf --set-interpreter /lib64/ld-linux-app.so.2 "$app/bin/app-probe"
BIONICX_PATCHELF=patchelf BIONICX_READELF=readelf \
    "$repo_dir/tools/rootfs-elf-fixup.sh" "$app" "$root" >/dev/null
[[ "$(patchelf --print-interpreter "$app/bin/app-probe")" == \
    "$root/usr/lib/ld-linux-aarch64.so.1" ]]
[[ "$(patchelf --print-rpath "$app/bin/app-probe")" == "$system_runpath" ]]
echo "rootfs ELF fixup: PASS"
