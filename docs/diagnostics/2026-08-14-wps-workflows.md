# WPS Writer / Sheets / Presentation workflows on the reconstructed seed

Device `01408BH601027129`, seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.

`examples/wps/x11-send-key.c` is a Gladio-adjacent glibc X11 client that
injects keys and clicks through XTEST (core `XSendEvent` is ignored by Qt).
`examples/wps/run-workflows.sh` then drives the three untraced WPS binaries
on the shared rootfs.

## Writer

Opened `BionicX-WF-Writer.docx`. After dismissing the formula-check /
guest / default-app overlays, XTEST typed `BionicX_WF_20260814` and
`BionicX_WF_20260814_clip`, copied the document, pasted a duplicate, and
saved. `verify-docx.sh` after save and after force-stop cold reopen:

```text
BXTEST PASS wps-docx archive=BionicX-WF-Writer.docx members=16 paragraphs=8
BXCONTENT BionicX_WF_20260814
BXCONTENT BionicX_WF_20260814_clip
BXFORMAT count 2 BionicX_WF_20260814
```

Ctrl+P opened the Qt dialog with `Name: bionicx-test` / `Status: Ready`.
Job `bionicx-test-14` / `d00014-001` is a 16846-byte PDF 1.4 (the export
artifact). The formula-symbol warning is still shown and is not suppressed.

## Sheets

`Book1.xlsx` was copied to `BionicX-WF-Sheets.xlsx`. After edit+save and
cold reopen:

```text
BXTEST PASS wps-xlsx archive=BionicX-WF-Sheets.xlsx members=16 cells=4
BXCELL A1 value=12 formula=
BXCELL A2 value=30 formula=
BXCELL A3 value=42 formula=SUM(A1:A2)
BXCELL H21 value=20260814 formula=
```

The dated marker landed in H21 rather than A4; the SUM formula and its
cached result 42 survived.

## Presentation

`BionicX-WF-Slides.pptx` kept both title runs. F5 entered a borderless
1920×1080 slideshow that renders `BionicX Presentation` / `glibc and X11
on Android`. Escape returned to the editor. Cold reopen still verifies:

```text
BXTEST PASS wps-pptx archive=BionicX-WF-Slides.pptx members=57 slide1_text_runs=2
```

## PDF

See `2026-08-14-wps-pdf-tiff.md`. Untraced `wpspdf` already rendered the
two-page fixture (`1/2`, page-1 text, thumbnails) after shared-rootfs
`libtiff.so.5`.

Evidence: `evidence/rebuild-2026-08-14/wps-workflows-20260814.log`,
`wps-writer-edited.png`, `wps-writer-print-dialog.png`,
`wps-writer-export.pdf`, `wps-writer-cold-reopen.png`,
`wps-sheets-edited.png`, `wps-sheets-cold-reopen.png`,
`wps-slides-slideshow.png`, `wps-slides-cold-reopen.png`.
