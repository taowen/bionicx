#!/usr/bin/env bash
# Point Mozilla GreD at the shared Debian libnssckbi.so, then run the NSS probe.
# App-only: the fixture/probe must not replace the device seed.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
files="/data/user/0/$package_id/files"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")

# Firefox (and Thunderbird) load libnssckbi.so from GreD, not as a bare SONAME.
# The Debian package ships bundled softoken without that module; the shared
# libnss3 copy already provides the builtin roots. A relative symlink in the
# package directory is not a per-app library copy.
for gred in usr/lib/firefox-esr usr/lib/thunderbird; do
    if "${adb[@]}" shell run-as "$package_id" \
            test -e "files/rootfs/$gred/libsoftokn3.so"; then
        if ! "${adb[@]}" shell run-as "$package_id" \
                test -e "files/rootfs/$gred/libnssckbi.so"; then
            "${adb[@]}" shell run-as "$package_id" ln -s \
                ../aarch64-linux-gnu/libnssckbi.so \
                "files/rootfs/$gred/libnssckbi.so"
        fi
    fi
done

builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
mkdir -p "$repo_dir/build"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
    examples/nss-ckbi-probe/nss-ckbi-probe.c \
    -o build/nss-ckbi-probe -ldl

patchelf --set-interpreter \
    "$root/usr/lib/ld-linux-aarch64.so.1" \
    "$repo_dir/build/nss-ckbi-probe"

tmp="/data/local/tmp/nss-ckbi-probe-$$"
"${adb[@]}" push "$repo_dir/build/nss-ckbi-probe" "$tmp" >/dev/null
"${adb[@]}" shell run-as "$package_id" mkdir -p files/apps/nss-ckbi-probe
"${adb[@]}" shell run-as "$package_id" cp "$tmp" \
    files/apps/nss-ckbi-probe/nss-ckbi-probe
"${adb[@]}" shell rm "$tmp"

dns_servers="${BIONICX_DNS_SERVERS:-}"
if [[ -z "$dns_servers" ]]; then
    dns_servers="$("${adb[@]}" shell getprop net.dns1 2>/dev/null | tr -d '\r')"
fi
if [[ -z "$dns_servers" ]]; then
    dns_servers=172.19.0.2
fi
result="$("${adb[@]}" shell run-as "$package_id" \
    "$files/bin/bionicx-exec" --cwd "$root" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$root" \
    --env "BIONICX_TMPDIR=$files/run/bxapt" \
    --env "BIONICX_DNS_SERVERS=${dns_servers:-8.8.8.8}" \
    --env "MOZILLA_FIVE_HOME=$root/usr/lib/firefox-esr" \
    -- "$files/apps/nss-ckbi-probe/nss-ckbi-probe" 2>&1 || true)"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY nss-ckbi passed=" <<<"$result"
echo "$result" | grep -E 'BXSUMMARY nss-ckbi passed=[0-9]+ failed=0' >/dev/null
