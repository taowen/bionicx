#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="${1:?usage: tools/prepare-rootfs-seed.sh OUTPUT_BUNDLE_DIR}"
output_dir="$(realpath -m "$output_dir")"
case "$output_dir/" in
    "$repo_dir/build/"*) ;;
    *) echo "output must be below $repo_dir/build: $output_dir" >&2; exit 2 ;;
esac

canonical="$repo_dir/build/rootfs-seed-bundle"
inputs=(
    "$repo_dir/examples/hello/build-bundle.sh"
    "$repo_dir/tools/build-android-glibc.sh"
    "$repo_dir/tools/build-rootfs-seed.sh"
    "$repo_dir/tools/build-gladio.sh"
    "$repo_dir/tools/check-glibc-symbol-floor.py"
    "$repo_dir/tools/install-trixie-rootfs-seed.sh"
    "$repo_dir/tools/relocate-shebangs.py"
    "$repo_dir/tools/rootfs-elf-fixup.sh"
)
while IFS= read -r path; do inputs+=("$path"); done < <(
    find "$repo_dir/runtime/glibc/2.41" -maxdepth 1 -type f | sort
)
input_id="$({
    # Preserve the stable input order while excluding clone-specific absolute
    # filenames from sha256sum's output.
    for input in "${inputs[@]}"; do
        sha256sum "$input" | cut -d' ' -f1
    done
    git -C "$repo_dir/third_party/gladio" rev-parse HEAD
} | sha256sum | cut -d' ' -f1)"

if [[ ! -f "$canonical/INPUT-ID" ]] || \
        [[ "$(<"$canonical/INPUT-ID")" != "$input_id" ]]; then
    case "$canonical/" in
        "$repo_dir/build/rootfs-seed-bundle/") rm -rf -- "$canonical" ;;
        *) echo "refusing to replace unexpected path: $canonical" >&2; exit 2 ;;
    esac
    mkdir -p "$canonical"
    "$repo_dir/tools/build-rootfs-seed.sh" "$canonical"
    printf '%s\n' "$input_id" > "$canonical/INPUT-ID"
fi

mkdir -p "$output_dir"
rm -rf -- "$output_dir/rootfs"
# Hard links keep independently inspectable bundles without multiplying the
# rootfs seed in the host workspace.
cp -al "$canonical/rootfs" "$output_dir/rootfs"
cp -a "$canonical/packages.tsv" "$canonical/manual-packages.txt" "$output_dir/"
printf '%s\n' "$output_dir/rootfs"
