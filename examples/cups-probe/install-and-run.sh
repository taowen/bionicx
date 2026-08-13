#!/usr/bin/env bash
# Start an app-private cupsd, add the bionicx-test file destination, and run
# the cupsGetDests probe. Does not replace the shared rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
files="/data/user/0/$package_id/files"
root="$files/rootfs"
server="$files/run/cups"
socket="$server/run/cups.sock"
output="$server/spool/bionicx-test.out"
adb=("${ADB:-adb}" -s "$serial")
bundle="${BIONICX_CUPS_BUNDLE:-$repo_dir/build/cups-probe-bundle}"

"$repo_dir/examples/cups-probe/build-bundle.sh" "$bundle"
python3 "$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/cups-probe.json"

run_as() {
    local command="run-as $package_id"
    local argument
    for argument in "$@"; do
        command+=" $(printf "'%s'" "${argument//\'/\'\\\'\'}")"
    done
    "${adb[@]}" shell "$command"
}

exec_rootfs() {
    run_as "$files/bin/bionicx-exec" \
        --cwd "$root" \
        --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
        --env "BIONICX_ROOTFS=$root" \
        --env "BIONICX_TMPDIR=$server/run" \
        --env "CUPS_SERVER=$socket" \
        --env "CUPS_SERVERROOT=$server" \
        --env "CUPS_STATEDIR=$server" \
        --env "CUPS_DATADIR=$root/usr/share/cups" \
        --env "PATH=$root/usr/sbin:$root/usr/bin:$root/sbin:$root/bin" \
        -- "$@"
}

run_as mkdir -p "$server/run" "$server/spool" "$server/cache"
run_as sh -c "printf '%s\n' \
    'LogLevel warn' \
    'Listen $socket' \
    'Browsing Off' \
    'WebInterface No' \
    'MaxLogSize 0' \
    'SystemGroup lpadmin' \
    > '$server/cupsd.conf'"
run_as sh -c "printf '%s\n' \
    'StateDir $server' \
    'CacheDir $server/cache' \
    'RequestRoot $server/spool' \
    'TempDir $server/run' \
    'FileDevice Yes' \
    > '$server/cups-files.conf'"

# Debian's cups-daemon does not ship the file: backend. Install the
# glibc helper before cupsd starts so file: jobs actually write output.
temporary_backend="/data/local/tmp/cups-file-backend-$$"
"${adb[@]}" push "$bundle/app/bin/file-backend" "$temporary_backend" >/dev/null
run_as mkdir -p files/rootfs/usr/lib/cups/backend
run_as cp "$temporary_backend" files/rootfs/usr/lib/cups/backend/file
run_as chmod 700 files/rootfs/usr/lib/cups/backend/file
"${adb[@]}" shell rm "$temporary_backend"

# Drop any leftover cupsd so this probe owns the private socket.
"${adb[@]}" shell "run-as $package_id sh -c 'kill \$(cat $server/cupsd.pid 2>/dev/null) 2>/dev/null; killall cupsd 2>/dev/null; true'"
sleep 0.3
run_as rm -f "$socket" "$server/cupsd.pid"

# bionicx-exec waits for session children, so start cupsd from a detached
# shell instead of the blocking helper.
"${adb[@]}" shell "run-as $package_id sh -c 'exec >/dev/null 2>&1; $files/bin/bionicx-exec --cwd $root --env LD_PRELOAD=$files/lib/libbionicx-runtime.so --env BIONICX_ROOTFS=$root --env BIONICX_TMPDIR=$server/run --env CUPS_SERVERROOT=$server --env CUPS_STATEDIR=$server --env CUPS_DATADIR=$root/usr/share/cups -- $root/usr/sbin/cupsd -f -c $server/cupsd.conf &'"
for _ in $(seq 1 20); do
    if run_as test -S "$socket"; then
        break
    fi
    sleep 0.2
done
run_as test -S "$socket"

exec_rootfs "$root/usr/sbin/lpadmin" -p bionicx-test -E \
    -v "file:$output" -m raw

temporary="/data/local/tmp/cups-probe-$$"
"${adb[@]}" push "$bundle/app/bin/cups-probe" "$temporary" >/dev/null
run_as mkdir -p files/apps/cups-probe/bin
run_as cp "$temporary" files/apps/cups-probe/bin/cups-probe
"${adb[@]}" shell rm "$temporary"

result="$(exec_rootfs "$files/apps/cups-probe/bin/cups-probe" || true)"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY wps-cups passed=" <<<"$result"
grep -Fq "failed=0" <<<"$result"
echo "cupsGetDests probe: PASS"
