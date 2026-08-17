#!/usr/bin/env bash
# Converted leftover X11 probes must use the shared seed, never a private rootfs.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

probes=(
    x11-probe
    pointer-grab-x11-probe
    keyboard-grab-x11-probe
    clipboard-x11-probe
    session-x11-probe
    font-xft-probe
    xrender-x11-probe
    xkb-x11-probe
    network-x11-probe
    runtime-probe
    save-set-x11-probe
    server-grab-x11-probe
)

for name in "${probes[@]}"; do
    example="$repo_dir/examples/$name"
    "$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/$name.json"
    if grep -F -- '--runtime-root' "$example/install-and-run.sh" >/dev/null; then
        echo "$name must not replace the shared seed" >&2
        exit 1
    fi
    if grep -F 'hello/build-bundle.sh' "$example/build-bundle.sh" >/dev/null; then
        echo "$name must not copy hello's private rootfs" >&2
        exit 1
    fi
    if grep -F 'resolve-elf-deps.py' "$example/build-bundle.sh" >/dev/null; then
        echo "$name must not assemble a private ELF closure" >&2
        exit 1
    fi
    chmod +x "$example/build-bundle.sh" "$example/install-and-run.sh"
done

echo "legacy X11 probes are seed-safe: PASS"
