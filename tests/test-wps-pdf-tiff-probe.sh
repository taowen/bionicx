#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
grep -F 'libtiff.so.5' \
    "$repo_dir/examples/wps-pdf-tiff-probe/wps-pdf-tiff-probe.c" >/dev/null
grep -F 'libpdfmain.so' \
    "$repo_dir/examples/wps-pdf-tiff-probe/wps-pdf-tiff-probe.c" >/dev/null
grep -F 'shared-libtiff5' \
    "$repo_dir/examples/wps-pdf-tiff-probe/wps-pdf-tiff-probe.c" >/dev/null
grep -F 'dlopen-pdfmain' \
    "$repo_dir/examples/wps-pdf-tiff-probe/wps-pdf-tiff-probe.c" >/dev/null
grep -F '/apps/' \
    "$repo_dir/examples/wps-pdf-tiff-probe/wps-pdf-tiff-probe.c" >/dev/null
grep -F 'passed=5 failed=0' \
    "$repo_dir/examples/wps-pdf-tiff-probe/install-and-run.sh" >/dev/null
grep -F 'bxapt' \
    "$repo_dir/examples/wps-pdf-tiff-probe/install-and-run.sh" >/dev/null
grep -F 'deb' \
    "$repo_dir/examples/wps-pdf-tiff-probe/install-and-run.sh" >/dev/null
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
grep -E '^libtiff5[[:space:]]+4\.2\.0-1\+deb11u5[[:space:]]+6896296ef6193ff77434c5d1d09dd9a333633f7a208ab1cc7de3b286d2d45824' \
    "$repo_dir/packages/external-arm64.tsv" >/dev/null
grep -E '^libwebp6[[:space:]]+0\.6\.1-2\.1\+deb11u2[[:space:]]+edeb260e528fecae77457a63a468e55837a98079fdd7f1e20e9813c358f8c755' \
    "$repo_dir/packages/external-arm64.tsv" >/dev/null
grep -F 'libtiff5' "$repo_dir/docs/NEW-DEVICE.md" >/dev/null
grep -F 'BionicX-PDF-Integration.pdf' \
    "$repo_dir/profiles/wps-pdf.json" >/dev/null
grep -F 'wps-pdf-live-page' \
    "$repo_dir/examples/wps/assert-live-pdf.py" >/dev/null
grep -F 'AcceptedEULA' \
    "$repo_dir/examples/wps/accepted-eula.conf" >/dev/null
grep -F 'accepted-eula.conf' \
    "$repo_dir/examples/wps/install-and-run-pdf.sh" >/dev/null
grep -F 'install-and-run.sh' \
    "$repo_dir/examples/wps/install-and-run-pdf.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/wps/install-and-run-pdf.sh" >/dev/null; then
    echo "wps-pdf install must not replace the shared seed" >&2
    exit 1
fi
echo "wps pdf tiff probe pins libtiff5 on the shared rootfs: PASS"
