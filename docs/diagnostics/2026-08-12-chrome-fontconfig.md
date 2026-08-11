# Chrome app-private Fontconfig

## Symptom

Chrome's first real X11 browser window was structurally correct, but all text
was blank. Logs contained `Cannot load default config file`, repeated `Could not
find any font`, and HarfBuzz reports with empty glyph ranges.

## Correction

The Chrome profile now selects `${APP}/etc/fonts/fonts.conf`. The app-private
configuration scans Android's readable `/system/fonts` and six statically copied
Liberation Sans/Serif/Mono regular/bold fallbacks. Its cache is kept below the
Chrome home directory, so installation and invalidation require neither root nor
a Linux rootfs mount.

`examples/chrome/install-open-fonts.sh` resolves the open Liberation faces using
host Fontconfig, uploads them to the private Chrome tree, installs the config,
invalidates only the dedicated cache, and verifies the device file count. Font
binaries are deliberately not stored in this repository.

## Device result

On x300, the next 15-second ordinary-Activity run had zero default-config,
missing-font, empty-glyph, FD-ownership, and network-service-crash messages.
Chrome rendered readable tab, omnibox, warning, restore bubble, button, and
link text. Android-injected input then rendered a complete network error page,
which exposed the next independent gap as `DNS_PROBE_FINISHED_BAD_CONFIG`.

See `evidence/chrome-fontconfig.log` and `evidence/chrome-fontconfig-ui.png`.
