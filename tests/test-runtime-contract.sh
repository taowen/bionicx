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
    "$repo_dir/native/runtime/sysv-shm.c" \
    -o "$test_dir/libbionicx-runtime.so" -ldl -pthread
mkdir -p "$test_dir/bin" "$test_dir/lib"
cc -O2 -Wall -Wextra -Werror \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" \
    -o "$test_dir/bin/runtime-contract-probe" \
    "$test_dir/libbionicx-runtime.so" -lutil -ldl -pthread \
    -Wl,-rpath,"$test_dir"
mkdir -p "$root/usr/lib/aarch64-linux-gnu"
cc -shared -fPIC -O2 -Wall -Wextra -Werror \
    "$repo_dir/tests/fixtures/runtime-dlopen.c" \
    -o "$root/opt/bionicx-runtime-dlopen.so"
cp "$root/opt/bionicx-runtime-dlopen.so" \
    "$root/usr/lib/aarch64-linux-gnu/libbionicx-runtime-dlopen.so"
cp "$root/opt/bionicx-runtime-dlopen.so" \
    "$root/usr/lib/aarch64-linux-gnu/libbionicx-so-one.so.1"
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

if grep -F 'with_chrome_child_arguments' \
        "$repo_dir/native/runtime/fhs-exec.c" >/dev/null; then
    echo "fhs-exec must not special-case Chrome argv" >&2
    exit 1
fi

BIONICX_ROOTFS="$root" \
BIONICX_APP="$test_dir/app" \
BIONICX_TMPDIR="$temporary" \
BIONICX_CHILD_FLAGS="--disable-crashpad-for-testing --use-angle=vulkan" \
BIONICX_CHILD_DROP_FLAGS="--disable-gpu-compositing" \
BIONICX_DNS_SERVERS="127.0.0.53,127.0.0.54" \
SSL_CERT_FILE=/captured-cert \
SSL_CERT_DIR=/captured-certs \
NODE_EXTRA_CA_CERTS=/captured-cert \
SHELL=/bin/sh \
HOME=/captured-home \
PATH=/usr/bin:/bin \
LANG=C.UTF-8 \
LD_PRELOAD="$test_dir/libbionicx-runtime.so" \
    "$test_dir/bin/runtime-contract-probe" "$root" "$temporary"
