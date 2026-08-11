# XKB core device metadata

After XKB event selection was implemented, WPS/Qt's only XKB startup warning
mapped exactly to extension minor opcode 24, `GetDeviceInfo`.

BionicX now returns a structurally complete 32-byte XKB reply plus the required
counted device-name payload. The core keyboard is device 3, is named
`BionicX keyboard`, owns its state, has no buttons or LED feedbacks, and
explicitly reports requested optional extension-device features as unsupported.
Invalid device selectors receive `BadValue`.

The genuine AArch64 glibc probe parses this reply through
`XkbGetDeviceInfo`, checks the name and device ID, and still finishes 5/5 with
zero X errors. See `evidence/x11-desktop-probe-xkb-device-info.log`.

The subsequent WPS run emits neither of its previous `qt.qpa.xcb` XKB
warnings. Qt proceeds to its next previously unreachable request,
`XKEYBOARD minor=17` (`GetNames`), recorded in
`evidence/wps-xkb-device-info-fixed.log`.
