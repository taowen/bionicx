#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
for profile in firefox-esr firefox-esr-online krita qbittorrent keepassxc; do
    "$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/$profile.json"
done
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/popular-apps/install-and-run.sh" >/dev/null; then
    echo "popular install must not replace the shared seed" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/productivity-apps/README.md" >/dev/null; then
    echo "productivity README must not tell callers to wipe the seed" >&2
    exit 1
fi

prod_bundle="$repo_dir/build/productivity-apps-bundle-probe"
pop_bundle="$repo_dir/build/popular-apps-bundle-probe"
"$repo_dir/examples/productivity-apps/build-bundle.sh" "$prod_bundle" >/dev/null
"$repo_dir/examples/popular-apps/build-bundle.sh" "$pop_bundle" >/dev/null
test -s "$prod_bundle/app/fixtures/bionicx-image.ppm"
test -s "$prod_bundle/app/fixtures/bionicx-page.html"
test -s "$prod_bundle/app/fixtures/firefox-online-user.js"
test -s "$pop_bundle/app/fixtures/bionicx-webseed.torrent"
test -s "$pop_bundle/app/fixtures/bionicx-motion-audio.avi"
echo "popular app profiles stay on the shared seed declarations: PASS"
