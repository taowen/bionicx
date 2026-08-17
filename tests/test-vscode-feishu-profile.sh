#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vscode.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/feishu.json"
if grep -F -- '--single-process' "$repo_dir/profiles/vscode.json" >/dev/null; then
    echo "vscode must use a real renderer process" >&2
    exit 1
fi
if grep -F -- '--single-process' "$repo_dir/profiles/feishu.json" >/dev/null; then
    echo "feishu must use a real renderer process" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null; then
    echo "electron-app install must not replace the shared seed" >&2
    exit 1
fi
echo "vscode and feishu profiles keep ANGLE/Vortek: PASS"
