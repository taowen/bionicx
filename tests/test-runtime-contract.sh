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
    -o "$test_dir/bin/runtime-contract-probe" -lutil
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
grep -F 'int statx(' "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'statx guest /bin/sh' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'inotify max_user_watches' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'redirect_inotify_sysctl' \
    "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'redirect_proc_stat' \
    "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'open /proc/stat' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'btime 0' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'redirect_etc_shells' \
    "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'rooted /etc/shells' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'inotify_add_watch guest /bin' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'int inotify_add_watch(' \
    "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'kernel_syscall6' \
    "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'SYS_close_range' "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'close_range must not CLOEXEC stdin' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'io_uring_setup' \
    "$repo_dir/native/runtime/android-kernel.c" >/dev/null
grep -F 'int getifaddrs(' \
    "$repo_dir/native/runtime/dns.c" >/dev/null
grep -F 'getifaddrs interfaces' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'SYS_openat' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'realpath("/var/lib/dpkg/status"' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'QSaveFile AT_EMPTY_PATH copy fallback' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'audit_open' "$repo_dir/native/runtime/identity.c" >/dev/null
grep -F 'current app user shell' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'SHELL overrides app user shell' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'userInfo().shell' "$repo_dir/native/runtime/identity.c" >/dev/null
grep -F 'synthetic_shell' "$repo_dir/native/runtime/identity.c" >/dev/null
grep -F 'remember_fake_link' "$repo_dir/native/runtime/fhs-path.c" >/dev/null
grep -F 'copied group lock must report nlink 2' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'audit_log_acct_message' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'audit-stubs' \
    "$repo_dir/examples/account-file-probe/account-file-probe.c" >/dev/null
grep -F 'group-lock-nlink' \
    "$repo_dir/examples/account-file-probe/account-file-probe.c" >/dev/null
grep -F 'account-file 6/6' \
    "$repo_dir/examples/account-file-probe/account-file-probe.c" >/dev/null
grep -F 'close_inherited_fds' \
    "$repo_dir/native/executor/bionicx-exec.c" >/dev/null
grep -F 'BIONICX_CHILD_FLAGS' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F '"SSL_CERT_FILE"' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F '"SSL_CERT_DIR"' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F '"NODE_EXTRA_CA_CERTS"' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F '"SHELL"' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F '"HOME"' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'char *getenv(' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'int clearenv(' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'int unsetenv(' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F '"TERM"' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'clearenv keeps SHELL' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F '/captured-cert' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'bionicx-execve:' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'keep_standard_fds' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'dup2(STDIN_FILENO, STDOUT_FILENO)' \
    "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'exec must copy the pty onto stdout and stderr' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'open("/dev/tty"' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'restore_runtime_environment();' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'exec after environ replace must restore HOME' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'trace_bash_startup' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'exec.log' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'execveat' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'pid_t forkpty(' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'TIOCSCTTY' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'forkpty.log' "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'forkpty@GLIBC_2.17' \
    "$repo_dir/native/runtime/glibc-interpose.map" >/dev/null
grep -F 'glibc-interpose.map' "$repo_dir/tools/build.sh" >/dev/null
grep -F 'forkpty child stdin is not a tty' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
grep -F 'BIONICX_CHILD_FLAGS on --type= helper' \
    "$repo_dir/tests/fixtures/runtime-contract-probe.c" >/dev/null
if grep -F 'with_chrome_child_arguments' \
        "$repo_dir/native/runtime/fhs-exec.c" >/dev/null; then
    echo "fhs-exec must not special-case Chrome argv" >&2
    exit 1
fi

BIONICX_ROOTFS="$root" \
BIONICX_APP="$test_dir/app" \
BIONICX_TMPDIR="$temporary" \
BIONICX_CHILD_FLAGS="--disable-crashpad-for-testing" \
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
