#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/evince-handy-gresource-probe.json"
grep -F '/sm/puri/handy/themes/shared.css' \
    "$repo_dir/examples/evince-handy-gresource-probe/evince-handy-gresource-probe.c" \
    >/dev/null
grep -F 'hdy_init' \
    "$repo_dir/examples/evince-handy-gresource-probe/evince-handy-gresource-probe.c" \
    >/dev/null
grep -F '.gresource.hdy' \
    "$repo_dir/examples/evince-handy-gresource-probe/README.md" >/dev/null
grep -F ' \.gresource' \
    "$repo_dir/tools/rootfs-elf-fixup.sh" >/dev/null
grep -F 'passed=6 failed=0' \
    "$repo_dir/examples/evince-handy-gresource-probe/install-and-run.sh" \
    >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/evince-handy-gresource-probe/install-and-run.sh" \
        >/dev/null; then
    echo "evince-handy-gresource-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/evince-handy-gresource-probe/install-and-run.sh"
echo "evince handy gresource probe covers libhandy theme CSS: PASS"
