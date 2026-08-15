#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/xi-x11-probe.json"
grep -F 'XListInputDevices' \
    "$repo_dir/examples/xi-x11-probe/xi-x11-probe.c" >/dev/null
grep -F 'XIGrabModeSync' \
    "$repo_dir/examples/xi-x11-probe/xi-x11-probe.c" >/dev/null
grep -F 'XIGrabButton' \
    "$repo_dir/examples/xi-x11-probe/xi-x11-probe.c" >/dev/null
grep -F 'XChangeDeviceProperty' \
    "$repo_dir/examples/xi-x11-probe/xi-x11-probe.c" >/dev/null
grep -F 'XIWarpPointer' \
    "$repo_dir/examples/xi-x11-probe/xi-x11-probe.c" >/dev/null
grep -F 'XISetFocus' \
    "$repo_dir/examples/xi-x11-probe/xi-x11-probe.c" >/dev/null
grep -F 'GET_DEVICE_PROPERTY' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XInputExtension.java" >/dev/null
grep -F 'XI_WARP_POINTER' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XInputExtension.java" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/xi-x11-probe/xi-x11-probe.c" >/dev/null; then
    echo "XI family probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=10 failed=0' \
    "$repo_dir/examples/xi-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/xi-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/xi-x11-probe/install-and-run.sh" >/dev/null; then
    echo "XI family probe must not replace the shared seed" >&2
    exit 1
fi
test ! -e "$repo_dir/examples/xi1-x11-probe/xi1-x11-probe.c"
test ! -e "$repo_dir/examples/xi2-passive-grab-x11-probe/xi2-passive-grab-x11-probe.c"
chmod +x "$repo_dir/examples/xi-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/xi-x11-probe/install-and-run.sh"
echo "XI family probe is a libXi client: PASS"
