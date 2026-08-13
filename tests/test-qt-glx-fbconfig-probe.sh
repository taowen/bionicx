#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/qt-glx-fbconfig-probe.json"
grep -F 'GLX_LEVEL, 0' \
    "$repo_dir/examples/qt-glx-fbconfig-probe/qt-glx-fbconfig-probe.c" >/dev/null
grep -F 'qt-qglx-spec' \
    "$repo_dir/examples/qt-glx-fbconfig-probe/qt-glx-fbconfig-probe.c" >/dev/null
grep -F 'qt-qglx-visual' \
    "$repo_dir/examples/qt-glx-fbconfig-probe/qt-glx-fbconfig-probe.c" >/dev/null
grep -F 'qt-gl33-context' \
    "$repo_dir/examples/qt-glx-fbconfig-probe/qt-glx-fbconfig-probe.c" >/dev/null
grep -F 'GLX_LEVEL' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/GLXExtension.java" \
    >/dev/null
grep -F 'GLADIO_X11_SOCKET' \
    "$repo_dir/profiles/qt-glx-fbconfig-probe.json" >/dev/null
grep -F 'SINGLE_FBCONFIG_ID' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/GLXExtension.java" \
    >/dev/null
grep -F 'qt-qglx-current' \
    "$repo_dir/examples/qt-glx-fbconfig-probe/qt-glx-fbconfig-probe.c" >/dev/null
grep -F 'qt-keepassxc-spec' \
    "$repo_dir/examples/qt-glx-fbconfig-probe/qt-glx-fbconfig-probe.c" >/dev/null
grep -F 'passed=6 failed=0' \
    "$repo_dir/examples/qt-glx-fbconfig-probe/install-and-run.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/qt-glx-fbconfig-probe/install-and-run.sh" \
        >/dev/null; then
    echo "qt-glx-fbconfig-probe must not replace the shared seed" >&2
    exit 1
fi
echo "qt glx fbconfig probe requires a SingleBuffer config for Qt: PASS"
