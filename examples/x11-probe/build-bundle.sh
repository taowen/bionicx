#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/x11-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir"/*) ;;
    *) echo "output must be inside the repository: $output_dir" >&2; exit 2 ;;
esac

# Reuse the audited Android-compatible glibc/libX11 closure from the minimal
# hello test. The probe remains a separately compiled, genuine glibc ELF.
"$repo_dir/examples/hello/build-bundle.sh" "$output_dir"
container_output="/work/${output_dir#"$repo_dir"/}"
podman run --rm --pull=newer --network host \
    --volume "$repo_dir:/work:Z" --workdir /work \
    docker.io/library/debian:trixie-slim \
    sh -eu -c "
        dpkg --add-architecture arm64
        apt-get update >/dev/null
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \\
            gcc-aarch64-linux-gnu libc6-dev-arm64-cross libx11-dev:arm64 >/dev/null
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \\
            examples/x11-probe/x11-probe.c \\
            -o '$container_output/app/bin/x11-probe' -lX11
    "

"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$output_dir/app/bin/x11-probe" \
    --search-root "$output_dir/rootfs/usr/lib" \
    --json "$output_dir/x11-probe-dependency-closure.json"
echo "$output_dir"
