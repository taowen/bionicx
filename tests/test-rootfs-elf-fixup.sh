#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d "$repo_dir/build/test-elf-fixup.XXXXXXXX")"
trap 'rm -rf -- "$test_dir"' EXIT
root="$test_dir/root"
alias_root="$test_dir/alias"
mkdir -p "$root/usr/bin" "$root/var/lib" "$alias_root/usr/lib/kept"

cp "$repo_dir/build/test-runtime-contract/runtime-contract-probe" \
    "$root/usr/bin/probe"
patchelf --set-interpreter /lib64/ld-linux-test.so.2 \
    --set-rpath "$root$alias_root/usr/lib/recovered:/usr/lib/redirected:\$ORIGIN/lib" \
    "$root/usr/bin/probe"

BIONICX_PATCHELF=patchelf BIONICX_ROOT_ALIAS="$alias_root" \
    "$repo_dir/tools/rootfs-elf-fixup.sh" "$root" /unused/loader >/dev/null

[[ "$(patchelf --print-interpreter "$root/usr/bin/probe")" == \
    /lib64/ld-linux-test.so.2 ]]
[[ "$(patchelf --print-rpath "$root/usr/bin/probe")" == \
    "$alias_root/usr/lib/recovered:$root/usr/lib/redirected:\$ORIGIN/lib" ]]
grep -F $'/usr/bin/probe\t' "$root/var/lib/bionicx/elf-fixups.tsv" >/dev/null
echo "rootfs ELF fixup: PASS"
