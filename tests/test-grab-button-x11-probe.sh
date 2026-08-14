#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/grab-button-x11-probe.json"
grep -F 'GrabModeSync' \
    "$repo_dir/examples/grab-button-x11-probe/grab-button-x11-probe.c" >/dev/null
if grep -F 'keyboardMode != 1 || confineTo != 0' \
        "$repo_dir/android/app/src/main/java/com/winlator/xserver/requests/GrabRequests.java" >/dev/null; then
    echo "GrabButton must accept keyboardMode Sync" >&2
    exit 1
fi
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/grab-button-x11-probe/grab-button-x11-probe.c" >/dev/null; then
    echo "grab-button probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=4 failed=0' \
    "$repo_dir/examples/grab-button-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/grab-button-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/grab-button-x11-probe/install-and-run.sh" >/dev/null; then
    echo "grab-button probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/grab-button-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/grab-button-x11-probe/install-and-run.sh"
echo "grab-button probe is a libX11 client: PASS"
