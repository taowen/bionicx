#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/allow-events-sync-x11-probe.json"
grep -F 'SyncPointer' \
    "$repo_dir/examples/allow-events-sync-x11-probe/allow-events-sync-x11-probe.c" >/dev/null
grep -F 'SyncKeyboard' \
    "$repo_dir/examples/allow-events-sync-x11-probe/allow-events-sync-x11-probe.c" >/dev/null
grep -F 'SyncBoth' \
    "$repo_dir/examples/allow-events-sync-x11-probe/allow-events-sync-x11-probe.c" >/dev/null
if awk '/public static void allowEvents/,/^    public static void /' \
        "$repo_dir/android/app/src/main/java/com/winlator/xserver/requests/GrabRequests.java" \
        | grep -F 'throw new BadImplementation()'; then
    echo "AllowEvents must accept SyncPointer/SyncKeyboard/SyncBoth" >&2
    exit 1
fi
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/allow-events-sync-x11-probe/allow-events-sync-x11-probe.c" >/dev/null; then
    echo "allow-events-sync probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=4 failed=0' \
    "$repo_dir/examples/allow-events-sync-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/allow-events-sync-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/allow-events-sync-x11-probe/install-and-run.sh" >/dev/null; then
    echo "allow-events-sync probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/allow-events-sync-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/allow-events-sync-x11-probe/install-and-run.sh"
echo "allow-events-sync probe is a libX11 client: PASS"
