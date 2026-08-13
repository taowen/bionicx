#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$repo_dir/build/test-rootfs-compat"
cd "$repo_dir"
root="/proc/$$/cwd/build/test-rootfs-compat/root"
temporary="/proc/$$/cwd/build/test-rootfs-compat/tmp"

mkdir -p "$test_dir" "$root" "$temporary"
find "$test_dir" -mindepth 1 -delete
mkdir -p "$root" "$temporary"

cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    "$repo_dir/native/compat/rootfs-path.c" \
    "$repo_dir/native/compat/rootfs-exec.c" \
    "$repo_dir/native/compat/rootfs-metadata.c" \
    -o "$test_dir/libbionicx-rootfs.so" -ldl
cc -O2 -Wall -Wextra -Werror \
    "$repo_dir/tests/fixtures/rootfs-compat-probe.c" \
    -o "$test_dir/rootfs-compat-probe"

BIONICX_ROOTFS="$root" \
BIONICX_TMPDIR="$temporary" \
PATH=/usr/bin:/bin \
LD_PRELOAD="$test_dir/libbionicx-rootfs.so" \
    "$test_dir/rootfs-compat-probe" "$root" "$temporary"
