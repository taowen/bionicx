#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/qt-gui-segv-probe.json"
grep -F 'XTEST' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XTestExtension.java" \
    >/dev/null
grep -F 'class XTestExtension' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XTestExtension.java" \
    >/dev/null
grep -F 'new XTestExtension' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/XServer.java" \
    >/dev/null
grep -F 'injectRawKey' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/XServer.java" \
    >/dev/null
grep -F 'width = 64' \
    "$repo_dir/android/app/src/main/cpp/gladiorenderer/src/gl_context.c" \
    >/dev/null
grep -F 'XQueryExtension' \
    "$repo_dir/examples/qt-gui-segv-probe/qt-gui-segv-probe.c" >/dev/null
grep -F 'XTestQueryExtension' \
    "$repo_dir/examples/qt-gui-segv-probe/qt-gui-segv-probe.c" >/dev/null
grep -F 'libXtst.so.6' \
    "$repo_dir/examples/qt-gui-segv-probe/qt-gui-segv-probe.c" >/dev/null
grep -F 'libkeepassxc-autotype-xcb.so' \
    "$repo_dir/examples/qt-gui-segv-probe/qt-gui-segv-probe.c" >/dev/null
grep -F 'glClear' \
    "$repo_dir/examples/qt-gui-segv-probe/qt-gui-segv-probe.c" >/dev/null
grep -F 'qt-draw-after-current' \
    "$repo_dir/examples/qt-gui-segv-probe/qt-gui-segv-probe.c" >/dev/null
grep -F 'passed=7 failed=0' \
    "$repo_dir/examples/qt-gui-segv-probe/install-and-run.sh" >/dev/null
grep -F 'GLADIO_X11_SOCKET' \
    "$repo_dir/profiles/qt-gui-segv-probe.json" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/qt-gui-segv-probe/install-and-run.sh" \
        >/dev/null; then
    echo "qt-gui-segv-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/qt-gui-segv-probe/install-and-run.sh"
echo "qt gui segv probe covers XTEST, autotype and post-makeCurrent draw: PASS"
