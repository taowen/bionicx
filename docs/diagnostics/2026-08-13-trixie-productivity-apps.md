# Debian trixie productivity cohort

This batch validates Firefox ESR, Evince and LibreOffice Writer from one
apt/dpkg-installed Debian trixie ARM64 rootfs.  It does not copy a private
dependency closure per application and does not use PRoot or Termux.

The rootfs is pinned to snapshot `20260811T000000Z`; its content ID is
`5bcd2b55882b6d61133489514e55aaf06b3135c318e8f2d065f756979f874768`.
The manifest contains 644 packages and the relocation gate checked 2,393 ELF
objects against the Debian GLIBC 2.41 floor.

## Capabilities found by controlled fixtures

- Evince required real MIT-SHM segment import, including non-zero image
  offsets, and `MIT-SHM GetImage`.  BionicX receives the glibc client's SysV
  shared-memory fd over an abstract Unix socket with `SCM_RIGHTS`; pixels are
  not copied through a fake protocol.
- LibreOffice stores its single-instance Unix socket below the literal
  `/tmp`, ignoring `TMPDIR`.  The `android-tmp` compatibility module relocates
  only that FHS path into the app-private Android cache directory.
- Writer startup exercised XRender radial gradients.  Document interaction
  then exposed core `ChangeActivePointerGrab` and XKB `GetIndicatorState`.
  Both now have protocol implementations instead of success stubs.
- Firefox ESR must use direct mode because XPCOM resolves its installation via
  `/proc/self/exe`.  A deterministic local HTML page rendered successfully.
  The test device had no usable external route, so this batch does not claim an
  online Firefox test.

## Device acceptance

All processes ran as the ordinary `io.taowen.bx` application UID on x300
`01408BH601027129`, without Frida or tracing in the accepted run.

| Application | Accepted workflow |
| --- | --- |
| Evince | Open a two-page PDF and render both pages |
| Firefox ESR | Open and render the controlled local HTML page |
| LibreOffice Writer | Open ODT, click into the document, insert `BionicX_saved_on_Android`, save with Ctrl+S, and verify that text in the pulled `content.xml` |

Screenshots are stored as `evidence/trixie-evince-page1.png`,
`evidence/trixie-evince-page2.png`, `evidence/trixie-firefox-esr-local.png`, and
`evidence/trixie-libreoffice-writer-saved.png`.

## Diagnostic lesson

An unsupported asynchronous X request can surface several interactions after
the actual trigger.  Logging sequence, major and minor opcode made the two
remaining blockers immediately actionable: core opcode 30 and XKEYBOARD minor
12.  The accepted run has neither error in logcat.
