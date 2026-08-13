#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$repo_dir/build/test-runtime-contract"
cd "$repo_dir"
root="/proc/$$/cwd/build/test-runtime-contract/root"
temporary="/proc/$$/cwd/build/test-runtime-contract/tmp"

mkdir -p "$test_dir" "$root" "$temporary"
find "$test_dir" -mindepth 1 -delete
mkdir -p "$root/etc" "$root/opt" "$temporary/run"

cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    "$repo_dir/native/runtime/android-kernel.c" \
    "$repo_dir/native/runtime/dns.c" \
    "$repo_dir/native/runtime/fhs-path.c" \
    "$repo_dir/native/runtime/fhs-exec.c" \
    "$repo_dir/native/runtime/fhs-metadata.c" \
    "$repo_dir/native/runtime/identity.c" \
    "$repo_dir/native/runtime/sysv-semaphore.c" \
    -o "$test_dir/libbionicx-runtime.so" -ldl
mkdir -p "$test_dir/bin" "$test_dir/lib"
cc -O2 -Wall -Wextra -Werror \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" \
    -o "$test_dir/bin/runtime-contract-probe"
mkdir -p "$root/usr/lib/aarch64-linux-gnu"
cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    "$repo_dir/tests/fixtures/runtime-dlopen.c" \
    -o "$root/opt/bionicx-runtime-dlopen.so"
cp "$root/opt/bionicx-runtime-dlopen.so" \
    "$root/usr/lib/aarch64-linux-gnu/libbionicx-runtime-dlopen.so"
cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    "$repo_dir/tests/fixtures/runtime-dlopen.c" \
    -o "$test_dir/lib/libbionicx-app-dlopen.so"
# Firefox loads GreD libnss3, then PR_LoadLibrary("libsoftokn3.so"). The
# system multiarch copy must not win once NSS_Initialize is already mapped.
mkdir -p "$test_dir/gred"
cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    "$repo_dir/tests/fixtures/runtime-nss-initialize.c" \
    -o "$test_dir/gred/libnss3.so"
cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    -DBIONICX_SOFTOKN_MARKER=1 \
    "$repo_dir/tests/fixtures/runtime-softokn-marker.c" \
    -o "$test_dir/gred/libsoftokn3.so"
cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    -DBIONICX_SOFTOKN_MARKER=2 \
    "$repo_dir/tests/fixtures/runtime-softokn-marker.c" \
    -o "$root/usr/lib/aarch64-linux-gnu/libsoftokn3.so"
mkdir -p "$test_dir/app/lib"
cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    -DBIONICX_SOFTOKN_MARKER=7 \
    "$repo_dir/tests/fixtures/runtime-softokn-marker.c" \
    -o "$test_dir/app/lib/libbionicx-app-gl.so"
cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    -DBIONICX_SOFTOKN_MARKER=8 \
    "$repo_dir/tests/fixtures/runtime-softokn-marker.c" \
    -o "$root/usr/lib/aarch64-linux-gnu/libbionicx-app-gl.so"

# GreD libnss3 must win over the multiarch libsoftokn3.so for bare dlopen.
grep -F 'libsoftokn3 must come from GreD' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'dlopen_from_loaded_nss' \
    "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'BIONICX_APP' \
    "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'BIONICX_FORCE_LINK_COPY' \
    "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'dpkg status-old copy fallback' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'statvfs("/usr/share/krita"' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'replenish_statvfs' \
    "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'AT_EMPTY_PATH' \
    "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'QSaveFile AT_EMPTY_PATH copy fallback' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null

BIONICX_ROOTFS="$root" \
BIONICX_APP="$test_dir/app" \
BIONICX_TMPDIR="$temporary" \
BIONICX_DNS_SERVERS="127.0.0.53,127.0.0.54" \
PATH=/usr/bin:/bin \
LD_PRELOAD="$test_dir/libbionicx-runtime.so" \
    "$test_dir/bin/runtime-contract-probe" "$root" "$temporary"
