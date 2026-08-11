# WPS Presentation edit, save, slideshow, and cold reopen

## Final profile

This verification used genuine AArch64 WPS Presentation 11.1.0.11720 with
`profiles/wps-presentation.json` on x300 `01408BH601027129`, Android API 34.
The application ran as the ordinary `io.taowen.bx` app UID through its
app-private glibc loader and BionicX X server. The final profile had no Frida,
debugger, signal diagnosis, file tracing, PRoot, Termux, or root execution. Its
log records `bionicx-exec: running untraced`.

`examples/wps/install-entrypoints.sh` first reproduced the private mode-`0700`
`office6/??` directory required by this WPS version's PPTX serializer. The
failure that established this narrow requirement is recorded separately in
`2026-08-12-wps-file-trace.md`.

## Functional workflow

WPS created a title slide and accepted these two text values through real X11
keyboard input:

```text
BionicX Presentation
glibc and X11 on Android
```

It saved the document as `BionicX-Presentation.pptx` in its private Documents
directory. The app-private file was 50,678 bytes. The host-side verifier read
it through `run-as`, tested the complete ZIP, required the package roots and
first slide, parsed DrawingML text nodes, and found both exact text runs:

```text
BXTEST PASS wps-pptx archive=BionicX-Presentation.pptx members=57 slide1_text_runs=2
BXSLIDE run=1 text=BionicX Presentation
BXSLIDE run=2 text=glibc and X11 on Android
```

The ribbon's **From Current Slide** action then entered a borderless 1920×1080
slideshow and rendered both values. Android `KEYCODE_ESCAPE` returned to the
editor, exercising the core keyboard-grab path instead of killing the process.

Finally, BionicX was force-stopped and cold-launched with the same formal
profile. WPS's own OpenFile dialog selected the saved PPTX, and the editor
rendered both persisted values. The cold-run log contains no unsupported X11
request, `BadImplementation`, fatal signal, or abnormal process exit.

## Retained evidence

- `evidence/wps-presentation-persistence.log`
- `evidence/wps-presentation-edited-saved.png`
- `evidence/wps-presentation-slideshow.png`
- `evidence/wps-presentation-cold-reopen.png`

The PPTX itself is user data and is deliberately not committed.
