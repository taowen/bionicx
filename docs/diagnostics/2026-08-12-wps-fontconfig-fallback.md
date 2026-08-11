# WPS deterministic font fallback and bold rendering

## Symptom

Writer correctly serialized `Bold_WPS_2026` with `<w:b/>` and `<w:bCs/>`, but
the cold-reopened Calibri bold run appeared as overlapping black blocks. Plain
text rendered normally. This separated the document-format path from the font
discovery/rendering path.

The first static-font attempt was a useful negative result: six Liberation
font files existed below `${APP}/usr/share/fonts/bionicx`, but WPS did not list
them and the rendering did not change. `fonts.conf` still pointed at the legacy
`files/wps-root` migration directory, which is a real stale directory on this
device rather than `${APP}`.

## Correction

`examples/wps/fonts.conf` now searches the actual profile application tree and
maps common unavailable Microsoft families to Liberation Sans, Serif, and Mono.
`examples/wps/install-open-fonts.sh` installs six host fonts through Android
`run-as`, verifies the device file count, and removes only its private
Fontconfig cache. It requires neither root nor a rootfs package manager and
does not redistribute WPS or font binaries in this repository.

## Result

After a force-stop and cold start, the native WPS OpenFile dialog reopened the
same `BionicX.docx`. `Bold_WPS_2026` rendered as distinct bold glyphs, while
the structural verifier still found the exact bold OOXML run. WPS's font menu
also listed Liberation Mono, Sans, and Serif plus DejaVu Math TeX Gyre; before
the path correction it skipped directly from Droid Sans Mono to Noto.

Evidence:

- `evidence/wps-fontconfig-bold-cold-reopen.png`
- `evidence/wps-fontconfig-discovery.png`
- `evidence/wps-fontconfig-fallback.log`

This proves deterministic fallback discovery and this Latin regular/bold path.
It does not yet prove CJK fallback, metric-perfect pagination, every font
style, or formula-symbol coverage. WPS still presents its formula-symbol health
warning on a fresh profile startup, so that remains a separate test target.
