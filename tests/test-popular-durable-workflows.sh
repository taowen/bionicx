#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runner="$repo_dir/examples/popular-workflows/run.sh"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/krita.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/krita-export.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/qbittorrent.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/keepassxc.json"
grep -F 'keepassxc-cli-probe' "$runner" >/dev/null
grep -F 'passed=6 failed=0' "$runner" >/dev/null
grep -F 'krita-glx-destroy-probe' "$runner" >/dev/null
grep -F 'passed=4 failed=0' "$runner" >/dev/null
grep -F 'qbit-payload-hash' "$runner" >/dev/null
grep -F 'qbit-fastresume' "$runner" >/dev/null
grep -F 'qbit-persist-after-stop' "$runner" >/dev/null
grep -F 'qbit-cold-payload' "$runner" >/dev/null
grep -F 'krita-export' "$runner" >/dev/null
grep -F '89 50 4e 47' "$runner" >/dev/null
grep -F '640, 480' "$runner" >/dev/null
grep -F 'krita-export.json' "$runner" >/dev/null
grep -F 'bionicx-saved.png' "$repo_dir/profiles/krita-export.json" >/dev/null
grep -F -- '--export' "$repo_dir/profiles/krita-export.json" >/dev/null
grep -F 'GLADIO_X11_SOCKET' "$repo_dir/profiles/krita-export.json" >/dev/null
grep -F 'keepassxc-deferred-open' "$repo_dir/profiles/keepassxc.json" >/dev/null
if grep -F -- '--runtime-root' "$runner" >/dev/null; then
    echo "popular-workflows must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$runner"
echo "popular durable workflows cover probes, persist and export: PASS"
