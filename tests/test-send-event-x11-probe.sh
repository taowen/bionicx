#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/send-event-x11-probe.json"
grep -F 'PointerWindow' \
    "$repo_dir/examples/send-event-x11-probe/send-event-x11-probe.c" >/dev/null
grep -F 'InputFocus' \
    "$repo_dir/examples/send-event-x11-probe/send-event-x11-probe.c" >/dev/null
grep -F 'XGrabServer' \
    "$repo_dir/examples/send-event-x11-probe/send-event-x11-probe.c" >/dev/null
grep -F 'resolveSendEventDestination' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/requests/WindowRequests.java" >/dev/null
grep -F 'input-focus-pointer-inside' \
    "$repo_dir/examples/send-event-x11-probe/send-event-x11-probe.c" >/dev/null
grep -F 'send-event-propagate' \
    "$repo_dir/examples/send-event-x11-probe/send-event-x11-probe.c" >/dev/null
grep -F 'isAncestorOf' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/requests/WindowRequests.java" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/send-event-x11-probe/send-event-x11-probe.c" >/dev/null; then
    echo "send-event probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=8 failed=0' \
    "$repo_dir/examples/send-event-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/send-event-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/send-event-x11-probe/install-and-run.sh" >/dev/null; then
    echo "send-event probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/send-event-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/send-event-x11-probe/install-and-run.sh"
echo "send-event probe is a two-connection X11 client: PASS"
