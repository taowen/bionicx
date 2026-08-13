#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$repo_dir/build/test-runtime-contract"
cd "$repo_dir"
root="/proc/$$/cwd/build/test-runtime-contract/root"
temporary="/proc/$$/cwd/build/test-runtime-contract/tmp"

mkdir -p "$test_dir" "$root" "$temporary"
find "$test_dir" -mindepth 1 -delete
mkdir -p "$root" "$temporary"

cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    "$repo_dir/native/runtime/android-kernel.c" \
    "$repo_dir/native/runtime/dns.c" \
    "$repo_dir/native/runtime/fhs-path.c" \
    "$repo_dir/native/runtime/fhs-exec.c" \
    "$repo_dir/native/runtime/fhs-metadata.c" \
    "$repo_dir/native/runtime/identity.c" \
    "$repo_dir/native/runtime/sysv-semaphore.c" \
    -o "$test_dir/libbionicx-runtime.so" -ldl
cc -O2 -Wall -Wextra -Werror \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" \
    -o "$test_dir/runtime-contract-probe"

BIONICX_ROOTFS="$root" \
BIONICX_TMPDIR="$temporary" \
PATH=/usr/bin:/bin \
LD_PRELOAD="$test_dir/libbionicx-runtime.so" \
    "$test_dir/runtime-contract-probe" "$root" "$temporary"
