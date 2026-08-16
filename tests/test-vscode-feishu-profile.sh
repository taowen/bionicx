#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/vscode.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/feishu.json"
grep -F -- '--no-sandbox' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F -- '--use-angle=vulkan' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F -- '--ozone-platform=x11' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F 'VK_ICD_FILENAMES' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F '"dbus"' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F '"SHELL": "${RUNTIME}/usr/bin/bash"' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F '"SSL_CERT_FILE": "${RUNTIME}/etc/ssl/certs/ca-certificates.crt"' \
    "$repo_dir/profiles/vscode.json" >/dev/null
grep -F '"SSL_CERT_DIR": "${RUNTIME}/etc/ssl/certs"' \
    "$repo_dir/profiles/vscode.json" >/dev/null
grep -F '"NODE_EXTRA_CA_CERTS": "${RUNTIME}/etc/ssl/certs/ca-certificates.crt"' \
    "$repo_dir/profiles/vscode.json" >/dev/null
grep -F '"TERM": "xterm-256color"' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F '"XMODIFIERS": "@im=none"' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F '"GTK_IM_MODULE": "gtk-im-context-simple"' \
    "$repo_dir/profiles/vscode.json" >/dev/null
grep -F '"SHELL": "/bin/sh"' "$repo_dir/profiles/feishu.json" >/dev/null
if grep -F -- '--single-process' "$repo_dir/profiles/vscode.json" >/dev/null; then
    echo "vscode must use a real renderer process" >&2
    exit 1
fi
grep -F -- '--no-sandbox' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F -- '--no-zygote' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F -- '--disable-crashpad-for-testing' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F 'CHROME_EXTRA_FLAGS' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F 'BIONICX_CHILD_FLAGS' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F 'BIONICX_LOG_EXEC' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F -- '--use-angle=vulkan' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F -- '--ozone-platform=x11' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F 'VK_ICD_FILENAMES' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F '"pulseaudio"' "$repo_dir/profiles/feishu.json" >/dev/null
if grep -F -- '--single-process' "$repo_dir/profiles/feishu.json" >/dev/null; then
    echo "feishu must use a real renderer process" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null; then
    echo "electron-app install must not replace the shared seed" >&2
    exit 1
fi
grep -F 'install git' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F '${HOME}/workspace' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F -- '--disable-workspace-trust' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F 'usr/bin/git' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'user.email' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'open-terminal-env.json' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'XK_dollar' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'ctrl-grave' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'XFlush' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'ctrl-backslash' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'ctrl-j' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'ctrl-f' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'secondarySideBar' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'titleBarStyle' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'useEditorAsCommitInput' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'profile-vscode/Backups' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'closePanel' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'existing.dispose' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'workbench.action.terminal.focus' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'onStartupFinished' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'createTerminal' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'closeAuxiliaryBar' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'chat.disableAIFeatures' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'hideOnStartup' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'shellIntegration.enabled' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'terminal.integrated.defaultLocation' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'source /data/user/0/io.taowen.bx/files/homes/vscode/.bashrc' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'IFS= read -r line' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F '/proc/self/fd/0' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'term.show(true)' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'echo hi' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F '%s\\n.\\n.\\n%s' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F "trap 'printf" \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'gpuAcceleration' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'commandsToSkipShell' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'focusFind' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F '"off"' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'stty cols 80' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'closeOtherEditors' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'stty -tostop' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'TerminalLocation.Editor' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'bionicx.open-terminal' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F '.vscode/extensions' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'ctrl-shift-p' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'chrome/build-bundle.sh' \
    "$repo_dir/examples/electron-apps/install-and-run.sh" >/dev/null
grep -F 'b20bfb21c5b3656e3411391c5c18df1782aedd7cd3bda0b9c08363ea261fca4b' \
    "$repo_dir/packages/external-arm64.tsv" >/dev/null
grep -F 'usr/share/code/code' "$repo_dir/profiles/vscode.json" >/dev/null
grep -F 'opt/bytedance/feishu/feishu' "$repo_dir/profiles/feishu.json" >/dev/null
grep -F 'da73070ae272d9d622a24c11ddd1409084de0601fc2bda6827d82424da78d9a4' \
    "$repo_dir/packages/external-arm64.tsv" >/dev/null
echo "vscode and feishu profiles keep ANGLE/Vortek: PASS"
