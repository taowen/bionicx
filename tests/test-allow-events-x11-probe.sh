#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/allow-events-x11-probe.json"
grep -F 'AsyncKeyboard' \
    "$repo_dir/examples/allow-events-x11-probe/allow-events-x11-probe.c" >/dev/null
grep -F 'ReplayKeyboard' \
    "$repo_dir/examples/allow-events-x11-probe/allow-events-x11-probe.c" >/dev/null
grep -F 'mode == 0 || mode == 3 || mode == 6' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/requests/GrabRequests.java" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/allow-events-x11-probe/allow-events-x11-probe.c" >/dev/null; then
    echo "allow-events probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=4 failed=0' \
    "$repo_dir/examples/allow-events-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/allow-events-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/allow-events-x11-probe/install-and-run.sh" >/dev/null; then
    echo "allow-events probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/allow-events-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/allow-events-x11-probe/install-and-run.sh"
echo "allow-events probe is a libX11 client: PASS"
