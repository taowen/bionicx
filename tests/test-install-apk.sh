#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
installer="$repo_dir/tools/install-apk.sh"
hello_runner="$repo_dir/examples/hello/install-and-run.sh"
guide="$repo_dir/docs/NEW-DEVICE.md"
test_dir="$repo_dir/build/test-install-apk"
rm -rf "$test_dir"
mkdir -p "$test_dir"

test -x "$installer"
grep -F 'pm install -r -t' "$installer" >/dev/null
grep -F 'adb install -r -t' "$installer" >/dev/null
grep -F 'V2509A' "$installer" >/dev/null
grep -F '607' "$installer" >/dev/null
grep -F '2289' "$installer" >/dev/null
grep -F '2462' "$installer" >/dev/null
grep -F 'bionicx-exec' "$installer" >/dev/null
grep -F 'libbionicx-runtime.so' "$installer" >/dev/null
grep -F 'keep existing' \
    "$repo_dir/android/app/src/main/java/com/winlator/BionicXActivity.java" >/dev/null
grep -F -- '--extract-only' "$installer" >/dev/null
grep -F 'tools/install-apk.sh' "$guide" >/dev/null
grep -F 'tools/install-apk.sh' "$hello_runner" >/dev/null
grep -F -- '--extract-only' "$hello_runner" >/dev/null
grep -F 'rootfs-seed-bundle/rootfs' "$hello_runner" >/dev/null
if grep -E -- '--runtime-root "\$bundle_dir/rootfs"' "$hello_runner" >/dev/null; then
    echo "hello must not replace the Debian seed with hello-bundle/rootfs" >&2
    exit 1
fi

# Missing APK is a hard error (do not fall through to adb).
if "$installer" --serial unused "$test_dir/missing.apk" \
        >"$test_dir/missing.out" 2>"$test_dir/missing.err"; then
    echo "missing APK was accepted" >&2
    exit 1
fi
grep -F 'missing apk' "$test_dir/missing.err" >/dev/null

# Strategy selection must not require a real device: mock getprop.
cat >"$test_dir/adb-vivo" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
log="${BIONICX_ADB_LOG:-/dev/null}"
printf '%s\n' "$*" >>"$log"
args=("$@")
if [[ "${args[0]:-}" == -s ]]; then
    args=("${args[@]:2}")
fi
case "${args[*]}" in
    "shell getprop ro.product.model") echo V2509A ;;
    "shell getprop ro.product.product.name") echo PD2509 ;;
    "shell getprop ro.product.device") echo PD2509 ;;
    "shell getprop ro.product.name") echo PD2509 ;;
    *) exit 0 ;;
esac
EOF
chmod +x "$test_dir/adb-vivo"
cat >"$test_dir/adb-other" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
log="${BIONICX_ADB_LOG:-/dev/null}"
printf '%s\n' "$*" >>"$log"
args=("$@")
if [[ "${args[0]:-}" == -s ]]; then
    args=("${args[@]:2}")
fi
case "${args[*]}" in
    "shell getprop ro.product.model") echo Pocket FIT ;;
    "shell getprop "*) echo ;;
    *) exit 0 ;;
esac
EOF
chmod +x "$test_dir/adb-other"

: >"$test_dir/dummy.apk"
ADB="$test_dir/adb-vivo" BIONICX_ADB_LOG="$test_dir/vivo.log" \
    "$installer" --dry-run --serial 10AFA31610002QH "$test_dir/dummy.apk" \
    >"$test_dir/vivo.out"
grep -F 'OriginOS confirm' "$test_dir/vivo.out" >/dev/null
grep -F 'pm install -r -t' "$test_dir/vivo.out" >/dev/null
grep -F '(607,2289)' "$test_dir/vivo.out" >/dev/null
grep -F '(607,2462)' "$test_dir/vivo.out" >/dev/null

ADB="$test_dir/adb-other" BIONICX_ADB_LOG="$test_dir/other.log" \
    "$installer" --dry-run --serial 01408BH601027129 "$test_dir/dummy.apk" \
    >"$test_dir/other.out"
grep -F 'adb install -r -t' "$test_dir/other.out" >/dev/null
if grep -F 'OriginOS' "$test_dir/other.out" >/dev/null; then
    echo "non-vivo device used OriginOS tap path" >&2
    exit 1
fi

echo "install-apk OriginOS automation and extract: PASS"
