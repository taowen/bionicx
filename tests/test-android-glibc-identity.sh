#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="$($repo_dir/tools/build-android-glibc.sh)"
probe="$repo_dir/build/android-glibc-identity-probe"
builder="$($repo_dir/tools/ensure-glibc-builder.sh)"

podman run --rm --userns=keep-id --volume "$repo_dir:/work:Z" \
    --workdir /work "$builder" aarch64-linux-gnu-gcc \
    -O2 -Wall -Wextra -Werror tests/fixtures/android-glibc-identity-probe.c \
    -o build/android-glibc-identity-probe
qemu-aarch64 "$output/ld-linux-aarch64.so.1" \
    --library-path "$output" "$probe"
