#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_HELLO_BUNDLE:-$repo_dir/build/hello-bundle}"
apk="${BIONICX_APK:-$repo_dir/build/bionicx-debug.apk}"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

# OriginOS needs a tap helper instead of a blocking `adb install`. Install the
# APK only when the package is missing; otherwise just stage bionicx-exec so
# bxapt normalize works before the first Activity launch.
install_apk=("$repo_dir/tools/install-apk.sh")
[[ -z "$serial" ]] || install_apk+=(--serial "$serial")
if "${adb[@]}" shell pm path io.taowen.bx >/dev/null 2>&1; then
    "${install_apk[@]}" --extract-only "$apk"
else
    "${install_apk[@]}" "$apk"
fi

# Rebuild deterministically before installation. A directory-presence check can
# silently reuse an older loader or libc after the runtime recipe changes.
"$repo_dir/examples/hello/build-bundle.sh" "$bundle_dir"
# hello-bundle/rootfs is only the X11/glibc closure. bxapt normalize needs a
# Debian seed with /usr/bin/apt-get; do not replace that seed with the bundle.
seed_root="${BIONICX_RUNTIME_ROOT:-$repo_dir/build/rootfs-seed-bundle/rootfs}"
install=("$repo_dir/tools/install-profile.sh" --profile "$repo_dir/profiles/hello.json"
    --app-root "$bundle_dir/app")
if [[ -d "$seed_root/usr" ]]; then
    install+=(--runtime-root "$seed_root")
fi
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W -n io.taowen.bx/com.winlator.BionicXActivity
