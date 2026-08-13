#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/keepassxc.json"
grep -F 'db-create' \
    "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh" >/dev/null
grep -F 'db-add' \
    "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh" >/dev/null
grep -F 'db-ls' \
    "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh" >/dev/null
grep -F 'db-show' \
    "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh" >/dev/null
grep -F 'db-reopen' \
    "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh" >/dev/null
grep -F 'db-persist' \
    "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh" >/dev/null
grep -F 'passed=6 failed=0' \
    "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh" >/dev/null
grep -F 'keepassxc-cli' \
    "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh" >/dev/null
grep -F 'Version>2.0' \
    "$repo_dir/examples/keepassxc-cli-probe/fixtures/bionicx.key" >/dev/null
grep -F '630DCD29' \
    "$repo_dir/examples/keepassxc-cli-probe/fixtures/bionicx.key" >/dev/null
grep -F 'bionicx.kdbx' "$repo_dir/profiles/keepassxc.json" >/dev/null
grep -F -- '--keyfile' "$repo_dir/profiles/keepassxc.json" >/dev/null
grep -F 'keepassxc-cli-probe' \
    "$repo_dir/examples/popular-apps/install-and-run.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh" \
        >/dev/null; then
    echo "keepassxc-cli-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh"
echo "keepassxc cli probe creates and reopens a key-file database: PASS"
