#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/soffice-doc-probe.json"
grep -F 'libvclplug_gtk3lo.so' \
    "$repo_dir/examples/soffice-doc-probe/soffice-doc-probe.c" >/dev/null
grep -F 'convert-txt' \
    "$repo_dir/examples/soffice-doc-probe/soffice-doc-probe.c" >/dev/null
grep -F 'LibreOffice Writer on BionicX' \
    "$repo_dir/examples/soffice-doc-probe/soffice-doc-probe.c" >/dev/null
grep -F 'WrappedTargetRuntimeException' \
    "$repo_dir/examples/soffice-doc-probe/README.md" >/dev/null
grep -F 'libreoffice-gtk3' \
    "$repo_dir/packages/trixie-popular.txt" >/dev/null
grep -F 'passed=8 failed=0' \
    "$repo_dir/examples/soffice-doc-probe/install-and-run.sh" >/dev/null
grep -F 'dlopen-swlo' \
    "$repo_dir/examples/soffice-doc-probe/soffice-doc-probe.c" >/dev/null
grep -F 'SAL_USE_VCLPLUGIN' \
    "$repo_dir/profiles/libreoffice-writer.json" >/dev/null
grep -F 'DISABLE_EXTENSION_SYNCHRONIZATION' \
    "$repo_dir/profiles/libreoffice-writer.json" >/dev/null
grep -F 'usr/bin/soffice' \
    "$repo_dir/profiles/libreoffice-writer.json" >/dev/null
grep -F 'DISABLE_EXTENSION_SYNCHRONIZATION' \
    "$repo_dir/examples/soffice-doc-probe/soffice-doc-probe.c" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/soffice-doc-probe/install-and-run.sh" >/dev/null; then
    echo "soffice-doc-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/soffice-doc-probe/install-and-run.sh"
echo "soffice document-load probe covers gtk3 VCL and convert-to: PASS"
