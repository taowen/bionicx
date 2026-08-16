#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/sync-x11-probe.json"
grep -F 'SYNC' \
    "$repo_dir/examples/sync-x11-probe/sync-x11-probe.c" >/dev/null
grep -F 'syncReqType = 2' \
    "$repo_dir/examples/sync-x11-probe/sync-x11-probe.c" >/dev/null
grep -F 'XGrabServer' \
    "$repo_dir/examples/sync-x11-probe/sync-x11-probe.c" >/dev/null
grep -F 'sync-await-grab' \
    "$repo_dir/examples/sync-x11-probe/sync-x11-probe.c" >/dev/null
grep -F 'XOpenDisplay' \
    "$repo_dir/examples/sync-x11-probe/sync-x11-probe.c" >/dev/null
grep -F 'CREATE_COUNTER' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/SyncExtension.java" \
    >/dev/null
grep -F 'new SyncExtension' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/XServer.java" \
    >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/sync-x11-probe/sync-x11-probe.c" >/dev/null; then
    echo "sync probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=8 failed=0' \
    "$repo_dir/examples/sync-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/sync-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/sync-x11-probe/install-and-run.sh" >/dev/null; then
    echo "sync probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/sync-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/sync-x11-probe/install-and-run.sh"
echo "sync probe is a two-connection X11 client: PASS"
