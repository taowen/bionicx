#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/xi1-x11-probe.json"
grep -F 'XListInputDevices' \
    "$repo_dir/examples/xi1-x11-probe/xi1-x11-probe.c" >/dev/null
grep -F 'XSelectExtensionEvent' \
    "$repo_dir/examples/xi1-x11-probe/xi1-x11-probe.c" >/dev/null
grep -F 'LIST_INPUT_DEVICES' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XInputExtension.java" >/dev/null
grep -F 'SELECT_EXTENSION_EVENT' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XInputExtension.java" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/xi1-x11-probe/xi1-x11-probe.c" >/dev/null; then
    echo "xi1 probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=4 failed=0' \
    "$repo_dir/examples/xi1-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/xi1-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/xi1-x11-probe/install-and-run.sh" >/dev/null; then
    echo "xi1 probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/xi1-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/xi1-x11-probe/install-and-run.sh"
echo "xi1 probe is a libXi client: PASS"
