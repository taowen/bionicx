#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/xi2-allow-events-x11-probe.json"
grep -F 'XIAllowEvents' \
    "$repo_dir/examples/xi2-allow-events-x11-probe/xi2-allow-events-x11-probe.c" >/dev/null
grep -F 'XIReplayDevice' \
    "$repo_dir/examples/xi2-allow-events-x11-probe/xi2-allow-events-x11-probe.c" >/dev/null
grep -F 'XI_ALLOW_EVENTS' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XInputExtension.java" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/xi2-allow-events-x11-probe/xi2-allow-events-x11-probe.c" >/dev/null; then
    echo "xi2-allow-events probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=4 failed=0' \
    "$repo_dir/examples/xi2-allow-events-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/xi2-allow-events-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/xi2-allow-events-x11-probe/install-and-run.sh" >/dev/null; then
    echo "xi2-allow-events probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/xi2-allow-events-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/xi2-allow-events-x11-probe/install-and-run.sh"
echo "xi2-allow-events probe is a libXi client: PASS"
