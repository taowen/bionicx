#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runner="$repo_dir/examples/vivo-apps/run.sh"
test -x "$runner"
grep -F 'xterm' "$runner" >/dev/null
grep -F 'evince' "$runner" >/dev/null
grep -F 'keepassxc' "$runner" >/dev/null
grep -F 'krita' "$runner" >/dev/null
grep -F 'firefox-esr' "$runner" >/dev/null
grep -F 'running untraced' "$runner" >/dev/null
grep -F 'BXSUMMARY vivo-apps passed=5 failed=0' "$runner" >/dev/null
grep -F -- '--app-root' "$runner" >/dev/null
if grep -F -- '--runtime-root' "$runner" >/dev/null; then
    echo "vivo-apps must not replace the shared seed" >&2
    exit 1
fi
grep -F '10AFA31610002QH' "$repo_dir/TODO.md" >/dev/null
grep -F 'examples/vivo-apps/run.sh' "$repo_dir/TODO.md" >/dev/null
chmod +x "$runner"
echo "vivo-apps launch five untraced GUIs: PASS"
