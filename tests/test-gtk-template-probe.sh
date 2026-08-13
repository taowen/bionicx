#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/gtk-template-probe.json"
grep -F 'gtkstatusbar.ui' \
    "$repo_dir/examples/gtk-template-probe/gtk-template-probe.c" >/dev/null
grep -F 'strchr(path, '"'"'/'"'"') == NULL' \
    "$repo_dir/native/runtime/fhs-exec.c" >/dev/null
grep -F 'ld.so.conf.bionicx' \
    "$repo_dir/tools/bxapt-ldconfig.sh" >/dev/null
grep -F 'GRESOURCE' \
    "$repo_dir/tools/rootfs-elf-fixup.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/gtk-template-probe/build-bundle.sh" >/dev/null
echo "GTK template probe and gresource-preserving fixup: PASS"
