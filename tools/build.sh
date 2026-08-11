#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
android_dir="$repo_dir/android"
assets_dir="$android_dir/app/src/main/assets/bionicx"
ndk_root="${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/29.0.14206865}"
ndk_bin="$ndk_root/toolchains/llvm/prebuilt/linux-x86_64/bin"
bionic_cc="$ndk_bin/aarch64-linux-android28-clang"
java17_home="${BIONICX_JAVA_HOME:-${JAVA_HOME:-}}"
if [[ -z "$java17_home" || ! -x "$java17_home/bin/java" ]] ||
        ! "$java17_home/bin/java" -version 2>&1 | head -1 | grep -q 'version "17'; then
    for candidate in \
        /usr/lib/jvm/java-17-openjdk \
        /usr/lib/jvm/java-17-openjdk-amd64 \
        /var/home/linuxbrew/.linuxbrew/Cellar/openjdk@17/17.0.19/libexec; do
        if [[ -x "$candidate/bin/java" ]]; then
            java17_home="$candidate"
            break
        fi
    done
fi

if [[ ! -x "$bionic_cc" ]]; then
    echo "missing Android NDK compiler: $bionic_cc" >&2
    exit 1
fi
if [[ ! -x "$java17_home/bin/java" ]]; then
    echo "missing JDK 17: $java17_home" >&2
    exit 1
fi

mkdir -p "$assets_dir/bin" "$assets_dir/lib" "$assets_dir/profiles" "$repo_dir/build"
"$bionic_cc" -Oz -Wall -Wextra -Werror \
    "$repo_dir/native/executor/bionicx-exec.c" \
    -o "$assets_dir/bin/bionicx-exec"
"$bionic_cc" -Oz -Wall -Wextra -Werror \
    "$repo_dir/native/tools/bionicx-relocate.c" \
    -o "$repo_dir/build/bionicx-relocate"

# Compatibility modules execute inside the glibc process and must themselves
# be glibc ELFs.  The container is a build tool only; it is never used on Android.
podman run --rm --pull=newer --network host \
    --volume "$repo_dir:/work:Z" --workdir /work \
    docker.io/library/debian:trixie-slim \
    sh -eu -c '
        apt-get update >/dev/null
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
            gcc-aarch64-linux-gnu libc6-dev-arm64-cross >/dev/null
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wall -Wextra -Werror \
            native/compat/wps-compat.c \
            -o android/app/src/main/assets/bionicx/lib/libbionicx-wps.so \
            -ldl -pthread
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wall -Wextra -Werror \
            native/compat/sigsys-report.c \
            -o android/app/src/main/assets/bionicx/lib/libbionicx-sigsys-report.so
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wall -Wextra -Werror \
            native/compat/android-seccomp.c \
            -o android/app/src/main/assets/bionicx/lib/libbionicx-android-seccomp.so
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wall -Wextra -Werror \
            native/compat/chrome.c \
            -o android/app/src/main/assets/bionicx/lib/libbionicx-chrome.so \
            -ldl
    '

cp "$repo_dir/profiles/hello.json" "$assets_dir/profiles/default.json"
JAVA_HOME="$java17_home" "$android_dir/gradlew" -p "$android_dir" \
    --no-daemon :app:assembleDebug
cp "$android_dir/app/build/outputs/apk/debug/app-debug.apk" \
    "$repo_dir/build/bionicx-debug.apk"

sha256sum "$assets_dir/bin/bionicx-exec" \
    "$assets_dir/lib/libbionicx-wps.so" \
    "$assets_dir/lib/libbionicx-sigsys-report.so" \
    "$assets_dir/lib/libbionicx-android-seccomp.so" \
    "$assets_dir/lib/libbionicx-chrome.so" \
    "$repo_dir/build/bionicx-relocate" \
    "$repo_dir/build/bionicx-debug.apk"
