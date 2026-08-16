#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/compositor-x11-probe.json"
grep -F 'RedirectSubwindows(root)' \
    "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null
grep -F 'set_window_shape_region' \
    "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null
grep -F 'shapeKind > 2' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XFixesExtension.java" >/dev/null
grep -F 'GetOverlayWindow' \
    "$repo_dir/examples/compositor-x11-probe/README.md" >/dev/null
grep -F 'raiseOverlay' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XComposite.java" >/dev/null
grep -F 'IsUnmapped' \
    "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null
grep -F 'XMapWindow' \
    "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null
grep -F 'XLowerWindow' \
    "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null
grep -F 'XGrabServer' \
    "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null
grep -F 'SET_CLIENT_INFO_ARB' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/GLXExtension.java" \
    >/dev/null
grep -F 'glx_set_client_info_arb' \
    "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null
grep -F 'overlay-child-output' \
    "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null; then
    echo "compositor probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=11 failed=0' \
    "$repo_dir/examples/compositor-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/compositor-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/compositor-x11-probe/install-and-run.sh" >/dev/null; then
    echo "compositor probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/compositor-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/compositor-x11-probe/install-and-run.sh"
echo "compositor probe is a two-connection xfwm4-shaped client: PASS"
