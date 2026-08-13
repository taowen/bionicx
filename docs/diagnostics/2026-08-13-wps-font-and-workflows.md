# WPS Fontconfig aliases and seed workflows

Device `01408BH601027129` kept seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.

`examples/wps-font-probe` is a genuine glibc Fontconfig client. Liberation
Sans/Serif already provide Latin letters and the formula operators WPS
checks (`± × π √ ∑`). Only after that coverage passed did the probe install
`50-bionicx-liberation-aliases.conf` so Calibri/Cambria/Arial/Times New
Roman resolve to those Liberation files. Result: `passed=6 failed=0`.
Proprietary fonts are not bundled. WPS still shows its own “formula
symbols might not be displayed” health dialog; that warning is not
suppressed.

Untraced Writer, Spreadsheets and Presentation opened dated seed fixtures
after `bxapt install libxslt1.1` and the Qt xcb plugin libraries
(`libxkbcommon-x11-0`, `libxcb-icccm4`, …). Structural verifiers passed:

```
BXTEST PASS wps-docx ... paragraphs=2
BXTEST PASS wps-xlsx ... A3=42 formula=SUM(A1:A2)
BXTEST PASS wps-pptx ... BionicX Slides 20260813
```

`wpspdf` still fails: it needs `libtiff.so.5`, which is not in Debian 13
trixie (only `libtiff6`).
