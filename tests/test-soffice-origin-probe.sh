#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
grep -F 'libreglo.so' \
    "$repo_dir/examples/soffice-origin-probe/soffice-origin-probe.c" >/dev/null
grep -F 'libuno_cppuhelpergcc3.so.3' \
    "$repo_dir/examples/soffice-origin-probe/soffice-origin-probe.c" >/dev/null
grep -F 'reglo-mapped' \
    "$repo_dir/examples/soffice-origin-probe/soffice-origin-probe.c" >/dev/null
grep -F 'OBJECT_DIR' \
    "$repo_dir/tools/rootfs-elf-fixup.sh" >/dev/null
grep -F 'object_directory' \
    "$repo_dir/tools/rootfs-elf-fixup.sh" >/dev/null
grep -F 'private_object_directory' \
    "$repo_dir/tools/rootfs-elf-fixup.sh" >/dev/null
grep -F 'prepend' \
    "$repo_dir/tools/rootfs-elf-fixup.sh" >/dev/null
grep -F 'soffice.bin' \
    "$repo_dir/examples/soffice-origin-probe/install-and-run.sh" >/dev/null
grep -F 'terminate_after_init' \
    "$repo_dir/examples/soffice-origin-probe/install-and-run.sh" >/dev/null
grep -F 'failed=0' \
    "$repo_dir/examples/soffice-origin-probe/install-and-run.sh" >/dev/null
grep -F 'libreoffice/program' \
    "$repo_dir/examples/soffice-origin-probe/README.md" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/soffice-origin-probe/install-and-run.sh" >/dev/null; then
    echo "soffice-origin-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/soffice-origin-probe/install-and-run.sh"
echo "soffice origin probe covers multiarch symlink \$ORIGIN: PASS"
