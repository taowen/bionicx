# WPS Writer PDF export

## Workflow and result

The formal untraced Writer profile opened the previously verified
`BionicX.docx` on x300 `01408BH601027129`, API 34. **Menu → Export to PDF**
selected the default app-private `Documents/BionicX.pdf` destination and all
pages. WPS completed the operation and displayed:

```text
Exporting PDF file is completed.
```

The output is a 23,624-byte PDF 1.7 document with one A4 page. `pdfinfo`
identifies its creator as `WPS Writer`. The existing device-boundary verifier
parsed it as PDF, required one page, and recovered exact source content:

```text
BXTEST PASS wps-pdf archive=BionicX.pdf pages=1 bytes=23624 sha256=ab58266f3a378858e15c5a88371192c759dc5ad057ec8854be7c316494173b32
BionicX_WPS_2026
abc_A
Bold_WPS_2026
BionicX_WPS_2026
```

This proves genuine WPS Writer PDF serialization, not just that the dialog
closed or a non-empty file appeared. See `evidence/wps-writer-pdf-export.log`
and `evidence/wps-writer-pdf-export.png`. The exported user document is not
committed.

## Separate launcher boundary

Clicking **Open File** in the completion dialog did not open the result. Writer
reported an error, while the compatibility log showed it attempted a shell
command beginning `bash  .../office6`; the compact runtime has no Bash. This is
an internal cross-module launcher issue after successful export. It is retained
as the next diagnostic boundary and is not hidden by the export claim.
