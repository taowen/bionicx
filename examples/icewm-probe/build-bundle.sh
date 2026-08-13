#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_dir="${1:-$repo_dir/build/icewm-probe-bundle}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

find "$output_dir" -mindepth 1 -delete 2>/dev/null || true
mkdir -p "$output_dir/app/bin" "$output_dir/app/share/icewm"

builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work "$builder_image" sh -eu -c '
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            examples/icewm-probe/icewm-window.c \
            -o "'"${output_dir#"$repo_dir/"}"'/app/bin/icewm-window" -lX11
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
            examples/icewm-probe/icewm-session.c \
            -o "'"${output_dir#"$repo_dir/"}"'/app/bin/icewm-session"
    '

cp "$repo_dir/examples/icewm-probe/preferences" \
    "$repo_dir/examples/icewm-probe/menu" "$output_dir/app/share/icewm/"
interpreter=/data/user/0/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
patchelf --set-interpreter "$interpreter" \
    "$output_dir/app/bin/icewm-window"
patchelf --set-interpreter "$interpreter" \
    "$output_dir/app/bin/icewm-session"
"$repo_dir/tools/check-glibc-symbol-floor.py" "$output_dir/app" --maximum 2.41
printf 'required_package=icewm\nrootfs_payload=none\n' \
    > "$output_dir/BUILD-INFO"
echo "$output_dir"
