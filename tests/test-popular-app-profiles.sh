#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
for profile in firefox-esr firefox-esr-online krita qbittorrent keepassxc; do
    "$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/$profile.json"
done
grep -F 'https://example.com/' \
    "$repo_dir/profiles/firefox-esr-online.json" >/dev/null
grep -F 'bionicx-page.html' \
    "$repo_dir/profiles/firefox-esr.json" >/dev/null
grep -F 'bionicx-image.ppm' "$repo_dir/profiles/krita.json" >/dev/null
grep -F 'GLADIO_X11_SOCKET' "$repo_dir/profiles/krita.json" >/dev/null
grep -F 'LD_LIBRARY_PATH=' \
    "$repo_dir/android/app/src/main/java/com/winlator/BionicXActivity.java" \
    >/dev/null
grep -F 'bionicx-webseed.torrent' \
    "$repo_dir/profiles/qbittorrent.json" >/dev/null
grep -F '"dbus"' "$repo_dir/profiles/krita.json" >/dev/null
grep -F '"dbus"' "$repo_dir/profiles/qbittorrent.json" >/dev/null
grep -F '"dbus"' "$repo_dir/profiles/keepassxc.json" >/dev/null
grep -F 'bionicx.kdbx' "$repo_dir/profiles/keepassxc.json" >/dev/null
grep -F -- '--keyfile' "$repo_dir/profiles/keepassxc.json" >/dev/null
grep -F 'keepassxc-deferred-open' "$repo_dir/profiles/keepassxc.json" >/dev/null
grep -F 'keepassxc-deferred-open' \
    "$repo_dir/examples/popular-apps/build-bundle.sh" >/dev/null
grep -F 'keepassxc-cli-probe' \
    "$repo_dir/examples/popular-apps/install-and-run.sh" >/dev/null
grep -F 'GLADIO_X11_SOCKET' "$repo_dir/profiles/keepassxc.json" >/dev/null
grep -F 'build-gladio.sh' \
    "$repo_dir/examples/popular-apps/install-and-run.sh" >/dev/null
grep -F 'firefox-esr' "$repo_dir/packages/trixie-popular.txt" >/dev/null
grep -F 'krita' "$repo_dir/packages/trixie-popular.txt" >/dev/null
grep -F 'qbittorrent' "$repo_dir/packages/trixie-popular.txt" >/dev/null
grep -F 'keepassxc' "$repo_dir/packages/trixie-popular.txt" >/dev/null
grep -F 'browser.aboutwelcome.enabled' \
    "$repo_dir/examples/productivity-apps/fixtures/firefox-online-user.js" \
    >/dev/null
grep -F 'network.process.enabled' \
    "$repo_dir/examples/productivity-apps/fixtures/firefox-online-user.js" \
    >/dev/null
grep -F 'MOZ_DISABLE_CONTENT_SANDBOX' \
    "$repo_dir/profiles/firefox-esr-online.json" >/dev/null
grep -F 'MOZ_FORCE_DISABLE_E10S' \
    "$repo_dir/profiles/firefox-esr-online.json" >/dev/null
grep -F 'firefox-online-user.js' \
    "$repo_dir/examples/productivity-apps/build-bundle.sh" >/dev/null
grep -F '../aarch64-linux-gnu/libnssckbi.so' \
    "$repo_dir/examples/popular-apps/install-and-run.sh" >/dev/null
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
echo "popular app profiles stay on the shared seed declarations: PASS"
