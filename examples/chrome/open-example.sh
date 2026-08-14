#!/usr/bin/env bash
# Type baidu.com into the running ANGLE Vulkan Chrome window.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
send="$repo_dir/examples/wps/send-key.sh"

title_ok() {
    "$send" list-windows 2>/dev/null | grep -Eq '百度|Baidu|baidu.com'
}

for _ in $(seq 1 50); do
    if "$send" list-windows 2>/dev/null | grep -Fq 'Google Chrome'; then
        break
    fi
    sleep 1
done

if title_ok; then
    echo "BXTEST PASS chrome-baidu-title"
    exit 0
fi

"$send" click-window 'Google Chrome' 0.40 0.07 || "$send" ctrl-l
sleep 0.4
"$send" ctrl-l
sleep 0.3
"$send" ctrl-a
sleep 0.2
"$send" type 'baidu.com'
sleep 0.3
"$send" return

for _ in $(seq 1 40); do
    if title_ok; then
        echo "BXTEST PASS chrome-baidu-title"
        exit 0
    fi
    sleep 1
done
echo "baidu.com window title did not appear" >&2
exit 1
