#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runner="$repo_dir/examples/reboot-recheck/run.sh"
grep -F 'am force-stop' "$runner" >/dev/null
grep -F 'reboot' "$runner" >/dev/null
grep -F 'keepassxc-cli-probe' "$runner" >/dev/null
grep -F 'passed=6 failed=0' "$runner" >/dev/null
grep -F 'glx-probe' "$runner" >/dev/null
grep -F 'BXSUMMARY host-glx' "$runner" >/dev/null
grep -F 'desktop-session' "$runner" >/dev/null
grep -F 'passed=7 failed=0' "$runner" >/dev/null
grep -F 'ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2' \
    "$runner" >/dev/null
if grep -F -- '--runtime-root' "$runner" >/dev/null; then
    echo "reboot-recheck must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$runner"
echo "reboot recheck runner covers force-stop, seed, CLI and desktop 7/7: PASS"
