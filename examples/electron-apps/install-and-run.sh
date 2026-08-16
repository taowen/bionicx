#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_ELECTRON_BUNDLE:-$repo_dir/build/chrome-host-bridge}"
profile="${BIONICX_ELECTRON_PROFILE:-$repo_dir/profiles/vscode.json}"
serial="${ANDROID_SERIAL:-}"

adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

profile_id="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "$profile")"
package_id=io.taowen.bx
files="/data/user/0/$package_id/files"
root="$files/rootfs"

vscode_git() {
    "${adb[@]}" shell run-as "$package_id" \
        "$files/bin/bionicx-exec" --cwd "$files/homes/vscode/workspace" \
        --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
        --env "BIONICX_ROOTFS=$root" \
        --env "HOME=$files/homes/vscode" \
        --env "PATH=$root/usr/bin:$root/bin" \
        -- "$root/usr/bin/git" "$@" >/dev/null
}

ensure_vscode_git() {
    local bxapt=("$repo_dir/tools/bxapt" install git)
    [[ -z "$serial" ]] || bxapt=("$repo_dir/tools/bxapt" --serial "$serial" install git)
    if ! "${adb[@]}" shell run-as "$package_id" \
            test -x files/rootfs/usr/bin/git >/dev/null 2>&1; then
        "${bxapt[@]}"
    fi
    "${adb[@]}" shell run-as "$package_id" sh -c \
        'mkdir -p files/homes/vscode/workspace'
    if ! "${adb[@]}" shell run-as "$package_id" \
            test -d files/homes/vscode/workspace/.git >/dev/null 2>&1; then
        "${adb[@]}" shell run-as "$package_id" sh -c \
            "printf 'bionicx vscode workspace\\n' > files/homes/vscode/workspace/README"
        "${adb[@]}" shell run-as "$package_id" sh -c \
            "printf '.vscode/open-terminal-env.json\\n' > files/homes/vscode/workspace/.gitignore"
        vscode_git init
        vscode_git add README .gitignore
    fi
    # Git cannot infer an email from Android's passwd/hostname, so commits
    # fail with "Please tell me who you are" until this is set.
    vscode_git config user.email "bionicx@localhost"
    vscode_git config user.name "BionicX"
}

ensure_vscode_settings() {
    local tmp="/data/local/tmp/vscode-user-settings-$$.json"
    local settings="$repo_dir/build/tmp/vscode-user-settings.json"
    mkdir -p "$repo_dir/build/tmp"
    cat > "$settings" <<'EOF'
{
  "workbench.secondarySideBar.defaultVisibility": "hidden",
  "workbench.startupEditor": "none",
  "window.titleBarStyle": "native",
  "chat.commandCenter.enabled": false,
  "chat.disableAIFeatures": true,
  "terminal.integrated.hideOnStartup": "never",
  "terminal.integrated.defaultLocation": "editor",
  "terminal.integrated.shellIntegration.enabled": false,
  "terminal.integrated.gpuAcceleration": "off",
  "terminal.integrated.persistentSessionReviveProcess": "never",
  "terminal.integrated.sendKeybindingsToShell": false,
  "terminal.integrated.commandsToSkipShell": [
    "workbench.action.terminal.focusFind",
    "actions.find",
    "workbench.action.showCommands"
  ],
  "extensions.autoUpdate": false,
  "extensions.autoCheckUpdates": false,
  "git.useEditorAsCommitInput": false
}
EOF
    "${adb[@]}" push "$settings" "$tmp" >/dev/null
    "${adb[@]}" shell run-as "$package_id" sh -c \
        "mkdir -p files/homes/vscode/profile-vscode/User && cp $tmp files/homes/vscode/profile-vscode/User/settings.json"
    "${adb[@]}" shell rm -f "$tmp"
    local keys_tmp="/data/local/tmp/vscode-keybindings-$$.json"
    local keybindings="$repo_dir/build/tmp/vscode-keybindings.json"
    cat > "$keybindings" <<'EOF'
[
  {
    "key": "f5",
    "command": "workbench.action.terminal.focus"
  },
  {
    "key": "ctrl+f",
    "command": "actions.find"
  }
]
EOF
    "${adb[@]}" push "$keybindings" "$keys_tmp" >/dev/null
    "${adb[@]}" shell run-as "$package_id" sh -c \
        "cp $keys_tmp files/homes/vscode/profile-vscode/User/keybindings.json"
    "${adb[@]}" shell rm -f "$keys_tmp"
    local bashrc_tmp="/data/local/tmp/vscode-bashrc-$$"
    local bashrc="$repo_dir/build/tmp/vscode-bashrc"
    cat > "$bashrc" <<'EOF'
PS1='bionicx@localhost:~$ '
echo "started $$" > /data/user/0/io.taowen.bx/cache/bashrc.log
EOF
    "${adb[@]}" push "$bashrc" "$bashrc_tmp" >/dev/null
    "${adb[@]}" shell chmod 644 "$bashrc_tmp"
    "${adb[@]}" shell run-as "$package_id" cp "$bashrc_tmp" files/homes/vscode/.bashrc
    "${adb[@]}" shell rm -f "$bashrc_tmp"
    # Ctrl/F5 accelerators do not reach Electron, and folderOpen tasks run
    # only once per folder. A tiny unpacked extension opens a terminal on
    # every window start.
    local ext_name=bionicx.open-terminal-0.0.1
    local ext_src="$repo_dir/build/tmp/$ext_name"
    mkdir -p "$ext_src"
    cat > "$ext_src/package.json" <<'EOF'
{
  "name": "open-terminal",
  "publisher": "bionicx",
  "displayName": "BionicX Open Terminal",
  "description": "Open an integrated terminal when the window starts.",
  "version": "0.0.1",
  "engines": { "vscode": "^1.80.0" },
  "activationEvents": ["onStartupFinished"],
  "main": "./extension.js",
  "capabilities": {
    "untrustedWorkspaces": { "supported": true },
    "virtualWorkspaces": true
  }
}
EOF
    cat > "$ext_src/extension.js" <<'EOF'
const fs = require('fs');
const os = require('os');
const path = require('path');
const vscode = require('vscode');
function reportPath(folder) {
  const dir = folder ? folder.uri.fsPath : '/tmp';
  const outDir = path.join(dir, '.vscode');
  fs.mkdirSync(outDir, { recursive: true });
  return path.join(outDir, 'open-terminal-env.json');
}
function activate() {
  let osShell = '';
  try { osShell = os.userInfo().shell; } catch (e) { osShell = String(e); }
  const shell = process.env.SHELL || '/bin/bash';
  const folder = vscode.workspace.workspaceFolders &&
    vscode.workspace.workspaceFolders[0];
  const report = {
    shell: process.env.SHELL || '',
    home: process.env.HOME || '',
    path: process.env.PATH || '',
    term: process.env.TERM || '',
    osShell: osShell,
    existsBinBash: fs.existsSync('/bin/bash'),
    existsPtmx: fs.existsSync('/dev/ptmx'),
    terminals: vscode.window.terminals.length
  };
  const write = () => {
    try {
      fs.writeFileSync(reportPath(folder), JSON.stringify(report, null, 2));
    } catch (e) {}
  };
  write();
  vscode.window.onDidOpenTerminal(t => {
    report.opened = t.name;
    report.terminalsAfter = vscode.window.terminals.length;
    write();
  });
  vscode.window.onDidCloseTerminal(t => {
    report.closed = t.name;
    report.exit = t.exitStatus ? t.exitStatus.code : null;
    report.exitReason = t.exitStatus ? t.exitStatus.reason : null;
    write();
  });
  const start = async () => {
    try {
      await vscode.commands.executeCommand(
          'workbench.action.closeAuxiliaryBar');
    } catch (e) {
      report.closeBarError = String(e);
    }
    try {
      for (const existing of vscode.window.terminals) {
        try { existing.dispose(); } catch (e) {}
      }
      const term = vscode.window.createTerminal({
        name: 'bash',
        location: vscode.TerminalLocation.Editor,
        shellPath: '/bin/bash',
        shellArgs: [
          '-c',
          "source /data/user/0/io.taowen.bx/files/homes/vscode/.bashrc; exec >/proc/self/fd/0 2>/proc/self/fd/0; stty -tostop -ixon 2>/dev/null; trap '' TTOU TTIN; trap 'printf \"\\n%s\" \"${PS1:-bionicx@localhost:~$ }\"' INT; printf '\\033[?2004l'; echo hi > /data/user/0/io.taowen.bx/cache/pty-eval.log; sleep 0.8; printf '%s\\n.\\n.\\n%s' hi \"${PS1:-bionicx@localhost:~$ }\"; while IFS= read -r line; do eval \"$line\"; printf '%s' \"${PS1:-bionicx@localhost:~$ }\"; stty cols 81 >/dev/null 2>&1; stty cols 80 >/dev/null 2>&1; printf '\\033[?25l\\033[?25h'; done"
        ]
      });
      term.show(true);
      try {
        await vscode.commands.executeCommand(
            'workbench.action.closeOtherEditors');
      } catch (e) {}
      try {
        await vscode.commands.executeCommand(
            'workbench.action.closePanel');
      } catch (e) {}
      try {
        await vscode.commands.executeCommand(
            'workbench.action.terminal.focus');
      } catch (e) {}
      setTimeout(() => {
        try { term.show(true); } catch (e) {}
      }, 4000);
      report.created = true;
    } catch (e) {
      report.createError = String(e);
    }
    await new Promise(r => setTimeout(r, 2500));
    report.terminalsAfter = vscode.window.terminals.length;
    report.names = vscode.window.terminals.map(t => t.name);
    report.exits = vscode.window.terminals.map(
        t => t.exitStatus ? t.exitStatus.code : null);
    write();
  };
  setTimeout(start, 2500);
}
function deactivate() {}
module.exports = { activate, deactivate };
EOF
    local ext_root="$files/homes/vscode/.vscode/extensions"
    local ext_json="$repo_dir/build/tmp/vscode-extensions.json"
    cat > "$ext_json" <<EOF
[{
  "identifier": {"id": "bionicx.open-terminal"},
  "version": "0.0.1",
  "location": {
    "\$mid": 1,
    "fsPath": "$ext_root/$ext_name",
    "external": "file://$ext_root/$ext_name",
    "path": "$ext_root/$ext_name",
    "scheme": "file"
  },
  "relativeLocation": "$ext_name",
  "metadata": {
    "installedTimestamp": 0,
    "pinned": true,
    "source": "resource",
    "isPreReleaseVersion": false
  }
}]
EOF
    local pkg_tmp="/data/local/tmp/bx-ot-package-$$.json"
    local js_tmp="/data/local/tmp/bx-ot-extension-$$.js"
    local json_tmp="/data/local/tmp/bx-ot-extensions-$$.json"
    "${adb[@]}" push "$ext_src/package.json" "$pkg_tmp" >/dev/null
    "${adb[@]}" push "$ext_src/extension.js" "$js_tmp" >/dev/null
    "${adb[@]}" push "$ext_json" "$json_tmp" >/dev/null
    "${adb[@]}" shell "run-as $package_id sh -c 'mkdir -p files/homes/vscode/.vscode/extensions/$ext_name && cp $pkg_tmp files/homes/vscode/.vscode/extensions/$ext_name/package.json && cp $js_tmp files/homes/vscode/.vscode/extensions/$ext_name/extension.js && cp $json_tmp files/homes/vscode/.vscode/extensions/extensions.json'"
    "${adb[@]}" shell rm -f "$pkg_tmp" "$js_tmp" "$json_tmp"
    # Drop hot-exit backups so injected keystrokes do not reopen as dirty files.
    "${adb[@]}" shell run-as "$package_id" sh -c \
        'rm -rf files/homes/vscode/profile-vscode/Backups'
}

if [[ "$profile_id" == vscode ]]; then
    ensure_vscode_git
    ensure_vscode_settings
fi

TMPDIR="$repo_dir/build/tmp" \
    "$repo_dir/examples/chrome/build-bundle.sh" "$bundle_dir"
install=("$repo_dir/tools/install-profile.sh"
    --profile "$profile" --app-root "$bundle_dir/app")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W -n io.taowen.bx/com.winlator.BionicXActivity
