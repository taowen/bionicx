# Diagnostic notebook

Create one Markdown file per concrete failure that still matters after the
fix. Record device/build identity, the exact profile and command, the
decisive log excerpt, root cause, fix commit, and the regression probe.
Do not keep a new note plus screenshot for every intermediate
one-opcode experiment once a machine-readable probe covers it.

A probe is a small standalone client that isolates one failure. It must
not start VS Code, WPS, LibreOffice Writer, VLC, KeePassXC, or a desktop
daemon. Name it after the app if that names the bug; still do not launch
the app.

Do not commit proprietary binaries, credentials, cookies, user documents,
or unredacted application-private data. Device recapture logs belong under
`build/evidence/` unless they are a current acceptance screenshot.
