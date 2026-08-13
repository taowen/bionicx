#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/qt-widgets-show-probe.json"
grep -F 'QWidgetPrivate::showChildren' \
    "$repo_dir/examples/qt-widgets-show-probe/README.md" >/dev/null
grep -F 'QMainWindow' \
    "$repo_dir/examples/qt-widgets-show-probe/qt-widgets-show-probe.cpp" \
    >/dev/null
grep -F 'simple.show' \
    "$repo_dir/examples/qt-widgets-show-probe/qt-widgets-show-probe.cpp" \
    >/dev/null
grep -F 'chrome.show' \
    "$repo_dir/examples/qt-widgets-show-probe/qt-widgets-show-probe.cpp" \
    >/dev/null
grep -F 'qt-widgets-show' \
    "$repo_dir/examples/qt-widgets-show-probe/qt-widgets-show-probe.cpp" \
    >/dev/null
grep -F 'passed=3 failed=0' \
    "$repo_dir/examples/qt-widgets-show-probe/install-and-run.sh" >/dev/null
grep -F 'QSystemTrayIcon' \
    "$repo_dir/examples/qt-widgets-show-probe/qt-widgets-show-probe.cpp" \
    >/dev/null
grep -F 'QTreeView' \
    "$repo_dir/examples/qt-widgets-show-probe/qt-widgets-show-probe.cpp" \
    >/dev/null
grep -F 'QT_XCB_GL_INTEGRATION' \
    "$repo_dir/profiles/qt-widgets-show-probe.json" >/dev/null
grep -F 'QT_STYLE_OVERRIDE' \
    "$repo_dir/profiles/qt-widgets-show-probe.json" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/qt-widgets-show-probe/install-and-run.sh" \
        >/dev/null; then
    echo "qt-widgets-show-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/qt-widgets-show-probe/install-and-run.sh"
echo "qt widgets show probe covers QWidgetPrivate::showChildren: PASS"
