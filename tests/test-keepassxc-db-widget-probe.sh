#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/keepassxc-db-widget-probe.json"
grep -F 'QStackedWidget' \
    "$repo_dir/examples/keepassxc-db-widget-probe/keepassxc-db-widget-probe.cpp" \
    >/dev/null
grep -F 'QSplitter' \
    "$repo_dir/examples/keepassxc-db-widget-probe/keepassxc-db-widget-probe.cpp" \
    >/dev/null
grep -F 'buildDatabaseWidgetLike' \
    "$repo_dir/examples/keepassxc-db-widget-probe/keepassxc-db-widget-probe.cpp" \
    >/dev/null
grep -F 'db-tree-show' \
    "$repo_dir/examples/keepassxc-db-widget-probe/keepassxc-db-widget-probe.cpp" \
    >/dev/null
grep -F 'welcome-show' \
    "$repo_dir/examples/keepassxc-db-widget-probe/keepassxc-db-widget-probe.cpp" \
    >/dev/null
grep -F 'QWidgetPrivate::showChildren' \
    "$repo_dir/examples/keepassxc-db-widget-probe/README.md" >/dev/null
grep -F 'bionicx.kdbx' \
    "$repo_dir/examples/keepassxc-db-widget-probe/README.md" >/dev/null
grep -F 'passed=4 failed=0' \
    "$repo_dir/examples/keepassxc-db-widget-probe/install-and-run.sh" >/dev/null
grep -F 'keepassxc-deferred-open' \
    "$repo_dir/examples/keepassxc-db-widget-probe/install-and-run.sh" >/dev/null
grep -F 'keepassxc-db-widget-gui' \
    "$repo_dir/examples/keepassxc-db-widget-probe/install-and-run.sh" >/dev/null
grep -F 'QT_XCB_GL_INTEGRATION' \
    "$repo_dir/profiles/keepassxc-db-widget-probe.json" >/dev/null
grep -F 'QT_STYLE_OVERRIDE' \
    "$repo_dir/profiles/keepassxc-db-widget-probe.json" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/keepassxc-db-widget-probe/install-and-run.sh" \
        >/dev/null; then
    echo "keepassxc-db-widget-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/keepassxc-db-widget-probe/install-and-run.sh"
echo "keepassxc DatabaseWidget show probe covers stacked/splitter tree: PASS"
