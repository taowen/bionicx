# WPS PDF controlled rendering, navigation, and cold reopen

## Controlled document

`examples/wps/build-pdf-fixture.py` creates a deterministic 1,110-byte PDF 1.4
file with two Letter-sized pages, a built-in Helvetica font, valid xref/trailer,
and four known text lines:

```text
BionicX PDF Page 1
glibc + X11 on Android
BionicX PDF Page 2
Navigation verified
```

The installer requires `pdfinfo` to report two pages and `pdftotext` to recover
every exact line before copying it to WPS's app-private Documents directory. It
then compares the host/device SHA-256 hash. The independent `verify-pdf.sh`
repeats page-count and exact-text checks across Android's `run-as` boundary.

```text
BXTEST PASS wps-pdf archive=BionicX-PDF-Integration.pdf pages=2 bytes=1110 sha256=687965144148cacad32a0fddc8fb8d14567c88a7af897f36e45e1954281d1af7
```

## Real WPS workflow

On x300 `01408BH601027129`, API 34, the formal untraced WPS PDF profile opened
the fixture through its own OpenFile dialog. It rendered page 1 at fit width,
including both exact text lines and two distinct page thumbnails. The next-page
button changed the page field from `1/2` to `2/2` and rendered both exact
second-page lines. Two zoom-in actions changed the displayed scale from
112.17% to 125% and visibly enlarged page 2.

BionicX was then force-stopped and cold-launched. The same OpenFile workflow
reopened the fixture at page `1/2` with both thumbnails and page-one text
visible. The cold-run log records `bionicx-exec: running untraced` and contains
no missing library, unsupported X11 request, `BadImplementation`, fatal signal,
or abnormal process exit.

This proves PDF import, page rendering, navigation, zoom, and cold reopen. It
does not yet claim PDF export from Writer/Spreadsheets/Presentation.

Retained evidence:

- `evidence/wps-pdf-render-navigation.log`
- `evidence/wps-pdf-page1.png`
- `evidence/wps-pdf-page2-zoom.png`
- `evidence/wps-pdf-cold-reopen.png`

The generated fixture is reproducible and therefore is not committed as a
binary artifact.
