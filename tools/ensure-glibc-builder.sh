#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
containerfile="$repo_dir/tools/container/Containerfile.glibc-arm64"
engine="${CONTAINER_ENGINE:-podman}"
definition_hash="$(sha256sum "$containerfile" | cut -c1-16)"
image="localhost/bionicx-glibc-arm64:$definition_hash"

if ! "$engine" image exists "$image"; then
    echo "building cached AArch64 glibc/X11 toolchain $image" >&2
    "$engine" build --network=host --pull=missing --tag "$image" \
        --file "$containerfile" "$repo_dir/tools/container" >&2
fi

printf '%s\n' "$image"
