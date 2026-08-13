#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/desktop-session.json"
grep -F 'usr/bin/icewm' "$repo_dir/profiles/desktop-session.json" >/dev/null
grep -F 'usr/bin/xterm' "$repo_dir/profiles/desktop-session.json" >/dev/null
grep -F 'usr/bin/mousepad' "$repo_dir/profiles/desktop-session.json" >/dev/null
grep -F 'desktop-session-launch' "$repo_dir/examples/desktop-session/desktop-session.c" >/dev/null
grep -F -- '--accept' "$repo_dir/profiles/desktop-session.json" >/dev/null
grep -F 'session-switch-xterm' "$repo_dir/examples/desktop-session/desktop-session.c" >/dev/null
grep -F 'session-resize-xterm' "$repo_dir/examples/desktop-session/desktop-session.c" >/dev/null
grep -F 'session-close-mousepad' "$repo_dir/examples/desktop-session/desktop-session.c" >/dev/null
grep -F 'session-reopen-mousepad' "$repo_dir/examples/desktop-session/desktop-session.c" >/dev/null
grep -F 'pulseaudio' "$repo_dir/profiles/desktop-session.json" >/dev/null
grep -F 'vulkan' "$repo_dir/profiles/desktop-session.json" >/dev/null
grep -F 'passed=7 failed=0' \
    "$repo_dir/examples/desktop-session/install-and-run.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/desktop-session/install-and-run.sh" >/dev/null; then
    echo "desktop-session must not replace the shared seed" >&2
    exit 1
fi
echo "desktop session profile launches two package apps: PASS"
