# WPS Presentation direct entrypoint

## Profile

`profiles/wps-presentation.json` launches the installed AArch64 `wpp` ELF with
the shared `wps-office` payload and home. It uses the same direct, app-private
glibc runtime and WPS compatibility module as Writer and Spreadsheets. The
profile itself does not enable signal diagnosis, a debugger, PRoot, Termux, or
root-only execution.

The reproducible `examples/wps/install-entrypoints.sh` preparation already
relocates `wpp`'s `PT_INTERP` to the app-private glibc loader and supplies the
audited Xtst dependency used across the suite.

## Device result

On x300 `01408BH601027129`, API 34, WPS Presentation launched untraced into its
full-screen home interface. Selecting New Document then created a genuine
`Presentation1` editing surface with one title slide, visible title/subtitle
placeholders, slide thumbnail, ribbon, notes area, and slideshow control.

The retained log contains `bionicx-exec: running untraced` and no missing ELF,
process exit, unsupported X request, or `BadImplementation`. See
`evidence/wps-presentation-entrypoint.log` and
`evidence/wps-presentation-new-slide.png`.

This checkpoint claims creation of a live editable presentation only. Text
editing, PPTX persistence, cold reopen, and slideshow mode are separate
functional checks.
