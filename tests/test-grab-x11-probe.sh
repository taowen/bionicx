#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/grab-x11-probe.json"
grep -F 'GrabModeSync' \
    "$repo_dir/examples/grab-x11-probe/grab-x11-probe.c" >/dev/null
grep -F 'SyncBoth' \
    "$repo_dir/examples/grab-x11-probe/grab-x11-probe.c" >/dev/null
grep -F 'confine' \
    "$repo_dir/examples/grab-x11-probe/grab-x11-probe.c" >/dev/null
grep -F 'getConfineWindow' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/GrabManager.java" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/grab-x11-probe/grab-x11-probe.c" >/dev/null; then
    echo "grab family probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=8 failed=0' \
    "$repo_dir/examples/grab-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/grab-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/grab-x11-probe/install-and-run.sh" >/dev/null; then
    echo "grab family probe must not replace the shared seed" >&2
    exit 1
fi
test ! -e "$repo_dir/examples/grab-key-x11-probe/grab-key-x11-probe.c"
test ! -e "$repo_dir/examples/allow-events-sync-x11-probe/allow-events-sync-x11-probe.c"
chmod +x "$repo_dir/examples/grab-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/grab-x11-probe/install-and-run.sh"
echo "grab family probe is a libX11 client: PASS"
