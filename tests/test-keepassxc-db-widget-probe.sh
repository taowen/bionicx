#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/keepassxc-db-widget-probe.json"
if grep -E 'keepassxc-deferred-open|pidof keepassxc' \
        "$repo_dir/examples/keepassxc-db-widget-probe/install-and-run.sh" \
        >/dev/null; then
    echo "keepassxc-db-widget-probe must not launch keepassxc" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/keepassxc-db-widget-probe/install-and-run.sh" \
        >/dev/null; then
    echo "keepassxc-db-widget-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/keepassxc-db-widget-probe/install-and-run.sh"
echo "keepassxc DatabaseWidget show probe covers stacked/splitter tree: PASS"
