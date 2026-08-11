# WPS regression after XRender glyph support

The new A8 glyph-set path changes shared Render protocol state and Drawable
alpha blending, so the controlled Xft pass alone is not sufficient evidence
for the real application.

The APK containing glyph-set support was switched from the controlled profile
back to the genuine AArch64 WPS profile. After a force-stop/cold-start, Writer
reached its home UI and its native OpenFile dialog reopened the existing
`BionicX.docx`. The regular paragraphs and `Bold_WPS_2026` remained visually
correct; no Render `BadImplementation` or BionicX protocol error appeared.

The independent OOXML verifier also retained its exact result:

```text
BXTEST PASS wps-docx archive=BionicX.docx members=16 paragraphs=3
BXFORMAT bold Bold_WPS_2026
```

Evidence is retained in `evidence/wps-after-xft-glyphs.png` and
`evidence/wps-after-xft-glyphs.log`. This is a non-regression checkpoint, not a
claim that WPS has switched its whole UI to the newly implemented Xft path.
