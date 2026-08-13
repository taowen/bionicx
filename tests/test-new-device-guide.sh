#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
guide="$repo_dir/docs/NEW-DEVICE.md"
test -s "$guide"
grep -F 'tools/build.sh' "$guide" >/dev/null
grep -F 'tools/build-rootfs-seed.sh' "$guide" >/dev/null
grep -F 'tools/install-apk.sh' "$guide" >/dev/null
grep -F 'pm install -r -t' "$guide" >/dev/null
grep -F 'tools/bxapt' "$guide" >/dev/null
grep -F 'packages/trixie-popular.txt' "$guide" >/dev/null
grep -F 'BIONICX_VIRTUAL_ROOT' "$guide" >/dev/null
grep -F 'set_robust_list' "$guide" >/dev/null
grep -F -- '--no-sandbox' "$guide" >/dev/null
grep -F 'dpkg --audit' "$guide" >/dev/null
grep -F '4 KiB' "$guide" >/dev/null
echo "new-device guide names seed rebuild, bxapt and kernel limits: PASS"
