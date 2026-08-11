# WPS Writer DOCX save and cold-reopen workflow

## Scope

The prior WPS checkpoint proved Qt/XKB input but stopped at an unsaved Writer
buffer. This run exercised the first durable application workflow using the
genuine AArch64 glibc WPS binary, its own Qt file dialogs, and BionicX's embedded
X server. No root, PRoot, Termux, Frida, or debugger was present in the runtime.

## Result

Writer created a document containing the exact paragraphs
`BionicX_WPS_2026` and `abc_A`. Its SaveAsFile dialog opened at
`${HOME}/Documents` and wrote `BionicX.docx`. The host-side
`examples/wps/verify-docx.sh` check crossed the Android `run-as` boundary,
tested all ZIP members, parsed `word/document.xml`, and found both paragraphs.

BionicX was then force-stopped and cold-started. WPS's OpenFile dialog returned
to the same private Documents directory, selected the saved file, and loaded
both lines correctly. The final state is retained in
`evidence/wps-docx-cold-reopen.png`; the structural check is in
`evidence/wps-docx-verify.log`.

This closes file creation, private-HOME path handling, Qt save/open dialogs,
OOXML write integrity, process restart, and document readback for Writer. It
does not yet cover normal graceful WPS shutdown, recent-file persistence,
formatting, clipboard, printing/PDF, or the Sheets and Presentation binaries.

## Newly observed non-blocker

Opening the document adds XFixes minor opcode 21 to the already known startup
gaps. The server returns `BadImplementation`, but WPS completes the load. It is
recorded as a future controlled XFixes regression target rather than being
silently treated as supported.
