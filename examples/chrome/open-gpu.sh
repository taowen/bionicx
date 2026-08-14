#!/usr/bin/env bash
# Official Chrome ignores chrome:// startup URLs. Type chrome://gpu
# into the omnibox of the running smoke window.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
send="$repo_dir/examples/wps/send-key.sh"

for _ in $(seq 1 50); do
    if "$send" list-windows 2>/dev/null | grep -Fq 'Google Chrome'; then
        break
    fi
    sleep 1
done

"$send" click-window 'Google Chrome' 0.40 0.07 || "$send" ctrl-l
sleep 0.4
"$send" ctrl-l
sleep 0.3
"$send" type 'chrome://gpu'
sleep 0.3
"$send" return

for _ in $(seq 1 30); do
    if "$send" list-windows 2>/dev/null | grep -Fq 'GPU Internals'; then
        echo "BXTEST PASS chrome-gpu-title"
        exit 0
    fi
    sleep 1
done
echo "chrome://gpu window title did not appear" >&2
exit 1
