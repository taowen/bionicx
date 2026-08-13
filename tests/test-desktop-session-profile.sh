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
echo "desktop session profile launches two package apps: PASS"
