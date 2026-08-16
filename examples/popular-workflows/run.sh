#!/usr/bin/env bash
# Durable untraced Krita / qBittorrent / KeePassXC workflows on the shared seed.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
files="/data/user/0/$package_id/files"
root="$files/rootfs"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
evidence="${BIONICX_EVIDENCE:-$repo_dir/build/evidence/popular-workflows}"
mkdir -p "$evidence"

exec_rootfs() {
    "${adb[@]}" shell run-as "$package_id" \
        "$files/bin/bionicx-exec" --cwd "$root" \
        --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
        --env "BIONICX_ROOTFS=$root" \
        --env "BIONICX_TMPDIR=$files/run/bxapt" \
        "$@"
}

kill_session() {
    "${adb[@]}" shell 'kill -9 $(pidof krita) $(pidof qbittorrent) $(pidof keepassxc) $(pidof bionicx-exec) 2>/dev/null; true'
    "${adb[@]}" shell am force-stop "$package_id"
}

shot() {
    local dest="$1"
    "${adb[@]}" exec-out screencap -p > "$dest"
    printf 'BXSHOT %s bytes=%s\n' "$(basename "$dest")" "$(wc -c < "$dest" | tr -d ' ')"
}

log="$evidence/popular-durable-workflows.log"
exec > >(tee "$log") 2>&1
echo "BXINFO seed"
"${adb[@]}" shell run-as "$package_id" cat files/rootfs/.bionicx-rootfs-seed-id

# --- KeePassXC: seed the kdbx fixture, then GUI, then show
ANDROID_SERIAL="$serial" \
BIONICX_SCREENSHOT="$evidence/keepassxc.png" \
    "$repo_dir/examples/popular-apps/install-and-run.sh" keepassxc \
    | tee "$evidence/keepassxc-seed.log"
grep -Fq "BXSUMMARY keepassxc-cli passed=6 failed=0" \
    "$evidence/keepassxc-seed.log"
sleep 8
shot "$evidence/keepassxc.png"
show="$(exec_rootfs \
    --env "HOME=$files/homes/keepassxc" \
    --env "PATH=$root/usr/bin:$root/bin" \
    --env "QT_QPA_PLATFORM=offscreen" \
    --env "QT_PLUGIN_PATH=$root/usr/lib/aarch64-linux-gnu/qt5/plugins" \
    --env "LANG=C.UTF-8" \
    -- "$root/usr/bin/keepassxc-cli" show \
        --key-file "$files/apps/keepassxc/fixtures/bionicx.key" \
        --no-password \
        -a Title -a UserName -a URL \
        "$files/apps/keepassxc/fixtures/bionicx.kdbx" login 2>&1 || true)"
printf '%s\n' "$show"
grep -Fxq login <<<"$show"
grep -Fxq bionicx <<<"$show"
grep -Fxq https://example.com <<<"$show"
echo "BXTEST PASS keepassxc-durable-show login/bionicx/example.com"

# --- qBittorrent: existing 256 KiB payload + fastresume, GUI, persist after stop
payload_host="$repo_dir/build/popular-apps-bundle/app/fixtures/bionicx-network-payload.bin"
if [[ ! -f "$payload_host" ]]; then
    "$repo_dir/examples/popular-apps/build-bundle.sh" \
        "$repo_dir/build/popular-apps-bundle" >/dev/null
fi
expected="$(sha256sum "$payload_host" | cut -d' ' -f1)"

ensure_qbit_payload() {
    "${adb[@]}" shell run-as "$package_id" mkdir -p \
        files/homes/qbittorrent/Downloads
    local actual
    actual="$("${adb[@]}" exec-out run-as "$package_id" \
        sha256sum files/homes/qbittorrent/Downloads/bionicx-network-payload.bin \
        2>/dev/null | awk '{print $1}' || true)"
    if [[ "$actual" != "$expected" ]]; then
        echo "BXINFO restore qbit payload have=${actual:-missing} want=$expected"
        local tmp="/data/local/tmp/bionicx-network-payload.bin"
        "${adb[@]}" push "$payload_host" "$tmp" >/dev/null
        "${adb[@]}" shell run-as "$package_id" cp "$tmp" \
            files/homes/qbittorrent/Downloads/bionicx-network-payload.bin
        "${adb[@]}" shell rm "$tmp"
    fi
}

ensure_qbit_payload
actual="$("${adb[@]}" exec-out run-as "$package_id" \
    sha256sum files/homes/qbittorrent/Downloads/bionicx-network-payload.bin \
    | awk '{print $1}')"
echo "BXINFO qbit-payload expected=$expected actual=$actual"
[[ "$actual" == "$expected" ]]
echo "BXTEST PASS qbit-payload-hash sha256=$actual"
"${adb[@]}" shell run-as "$package_id" \
    test -s files/homes/qbittorrent/.local/share/qBittorrent/BT_backup/91268fb792fd9160fb854060db0efbcd07c7ae24.fastresume
echo "BXTEST PASS qbit-fastresume"

ANDROID_SERIAL="$serial" \
BIONICX_SCREENSHOT="$evidence/qbittorrent.png" \
    "$repo_dir/examples/popular-apps/install-and-run.sh" qbittorrent
sleep 8
shot "$evidence/qbittorrent.png"
kill_session
sleep 1
after="$("${adb[@]}" exec-out run-as "$package_id" \
    sha256sum files/homes/qbittorrent/Downloads/bionicx-network-payload.bin \
    | awk '{print $1}')"
[[ "$after" == "$expected" ]]
"${adb[@]}" shell run-as "$package_id" \
    test -s files/homes/qbittorrent/.local/share/qBittorrent/BT_backup/91268fb792fd9160fb854060db0efbcd07c7ae24.fastresume
echo "BXTEST PASS qbit-persist-after-stop sha256=$after"

ANDROID_SERIAL="$serial" \
BIONICX_SCREENSHOT="$evidence/qbittorrent-cold.png" \
    "$repo_dir/examples/popular-apps/install-and-run.sh" qbittorrent
sleep 6
shot "$evidence/qbittorrent-cold.png"
cold="$("${adb[@]}" exec-out run-as "$package_id" \
    sha256sum files/homes/qbittorrent/Downloads/bionicx-network-payload.bin \
    | awk '{print $1}')"
[[ "$cold" == "$expected" ]]
echo "BXTEST PASS qbit-cold-payload sha256=$cold"

# --- Krita: GLX NULL-destroy, then GUI open, then export as the primary process
ANDROID_SERIAL="$serial" \
    "$repo_dir/examples/krita-glx-destroy-probe/install-and-run.sh" \
    | tee "$evidence/krita-glx-destroy-probe.log"
grep -Fq "BXSUMMARY krita-glx-destroy passed=4 failed=0" \
    "$evidence/krita-glx-destroy-probe.log"

ANDROID_SERIAL="$serial" \
BIONICX_SCREENSHOT="$evidence/krita.png" \
    "$repo_dir/examples/popular-apps/install-and-run.sh" krita
sleep 10
shot "$evidence/krita.png"

"${adb[@]}" shell run-as "$package_id" mkdir -p files/homes/krita/Documents
"${adb[@]}" shell run-as "$package_id" rm -f \
    files/homes/krita/Documents/bionicx-saved.png
# Reuse files/apps/krita (Gladio + PPM). App-only profile switch.
kill_session
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/krita-export.json" \
    --serial "$serial"
"${adb[@]}" logcat -c
"${adb[@]}" shell am start -W \
    -n "$package_id/com.winlator.BionicXActivity" >/dev/null
"${adb[@]}" shell cmd statusbar collapse >/dev/null 2>&1 || true

size=""
for _ in $(seq 1 45); do
    size="$("${adb[@]}" shell run-as "$package_id" \
        stat -c %s files/homes/krita/Documents/bionicx-saved.png 2>/dev/null \
        | tr -d '\r')"
    if [[ "$size" =~ ^[0-9]+$ ]] && [[ "$size" -gt 100 ]]; then
        break
    fi
    sleep 2
done
if [[ "$size" =~ ^[0-9]+$ ]] && [[ "$size" -gt 100 ]]; then
    echo "BXTEST PASS krita-export bytes=$size"
else
    echo "BXTEST FAIL krita-export bytes=${size:-missing}"
    "${adb[@]}" logcat -d -v brief | grep -E \
        'BionicX|krita|FATAL|fatal|Error:|exited' | tail -n 80 || true
    exit 1
fi
sig="$("${adb[@]}" exec-out run-as "$package_id" \
    dd if=files/homes/krita/Documents/bionicx-saved.png bs=8 count=1 2>/dev/null \
    | od -An -tx1)"
echo "BXINFO krita-export-head $sig"
echo "$sig" | grep -q '89 50 4e 47'
"${adb[@]}" exec-out run-as "$package_id" \
    cat files/homes/krita/Documents/bionicx-saved.png \
    > "$evidence/krita-export.png"
python3 - "$evidence/krita-export.png" <<'PY'
import struct, sys
data = open(sys.argv[1], "rb").read()
if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
    raise SystemExit("not a PNG")
width, height = struct.unpack(">II", data[16:24])
if (width, height) != (640, 480):
    raise SystemExit(f"size {width}x{height}")
print(f"BXTEST PASS krita-export-png 640x480 bytes={len(data)}")
PY
echo "BXSHOT krita-export.png bytes=$(wc -c < "$evidence/krita-export.png" | tr -d ' ')"

echo "BXSUMMARY popular-durable keepassxc+qbit+krita"
