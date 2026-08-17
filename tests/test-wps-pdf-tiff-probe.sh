#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/wps-pdf-tiff-probe/install-and-run.sh" >/dev/null; then
    echo "wps-pdf-tiff-probe must not replace the shared seed" >&2
    exit 1
fi
if grep -E 'files/apps/.*/usr/lib' \
        "$repo_dir/examples/wps-pdf-tiff-probe/install-and-run.sh" >/dev/null; then
    echo "wps-pdf-tiff-probe must not copy libraries into files/apps" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/wps/install-and-run-pdf.sh" >/dev/null; then
    echo "wps-pdf install must not replace the shared seed" >&2
    exit 1
fi
echo "wps pdf tiff probe pins libtiff5 on the shared rootfs: PASS"
