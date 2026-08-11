# WPS Writer clipboard, save, and cold-reopen workflow

## Scope

This run used the genuine AArch64 glibc WPS Writer process on Android, without
root, PRoot, Termux, Frida, or a debugger. The first paragraph of the existing
`BionicX.docx` was selected in Writer, copied with its Home-ribbon Copy action,
and pasted as a new fourth paragraph with the ribbon's Keep Text Only action.
This exercises the real WPS editor and X11 selection owner rather than changing
the OOXML archive from the host.

## Durable result

Writer's status bar advanced from three to four words and rendered the duplicate
paragraph. After the document was saved, `verify-docx.sh` read the app-private
file through Android `run-as`, tested the ZIP archive, parsed
`word/document.xml`, and reported:

```text
BXTEST PASS wps-docx archive=BionicX.docx members=16 paragraphs=4
BXCONTENT BionicX_WPS_2026
BXCONTENT abc_A
BXCONTENT Bold_WPS_2026
BXCONTENT BionicX_WPS_2026
BXFORMAT bold Bold_WPS_2026
BXFORMAT bold BionicX_WPS_2026
BXFORMAT count 2 BionicX_WPS_2026
```

BionicX was then force-stopped and cold-started. WPS returned to its home UI,
the native OpenFile dialog selected the same private document, and Writer
rendered all four paragraphs. The structural verifier passed again after the
cold reopen. Evidence is retained as
`evidence/wps-copy-paste-saved.png`,
`evidence/wps-copy-paste-cold-reopen.png`, and
`evidence/wps-copy-paste-verify.log`.

## Newly isolated protocol gap

Opening the Paste drop-down caused WPS to issue core request opcode 31:

```text
WinlatorXRequest: seq=5765 opcode=31 data=0 bytes=12
WinlatorXRequest: unsupported opcode=31 data=0
java.lang.UnsupportedOperationException: Unsupported opcode 31.
```

Opcode 31 is `GrabKeyboard`. The menu remained mouse-operable, so this was not
the cause of a failed paste, but treating it as `BadImplementation` loses the
keyboard exclusivity expected by general X11 menus and dialogs. It is therefore
the next controlled core-X11 regression target, together with opcode 32
`UngrabKeyboard`; WPS success here is not used to hide that missing semantic.
