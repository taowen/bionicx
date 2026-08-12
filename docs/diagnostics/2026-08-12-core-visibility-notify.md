# Core MapSubwindows and initial visibility

Unmodified Debian ARM64 IceWM created correctly sized, mapped `TitleBar`,
`Close`, `Minimize`, `Maximize`, and `SysMenu` windows, but Android showed only
the client areas and frame borders. A live recursive observer established that
the geometry and renderer traversal were present. `XGetImage` then separated
the server-side content from Android composition:

```text
before: TitleBar nonzero=0/12400; Close nonzero=0/400
```

The taskbar selected `ExposureMask` and painted normally. Decoration windows
instead selected `VisibilityChangeMask` (`events=0x21203f`). BionicX exposed
that mask but never sent core event 15. Its `MapSubwindows` implementation also
recursively mapped descendants and the target itself, and emitted `Expose`
while descendants were still unviewable.

`MapSubwindows` now maps only the target's direct children. Mapping an ancestor
walks descendants that become viewable, sends `Expose` to interested clients,
and sends `VisibilityNotify(Unobscured)` to visibility listeners. The genuine
glibc/libX11 core probe keeps the parent unmapped while mapping its children and
asserts the precise transition:

```text
BXTEST PASS map-subwindows-exposure before=0/1/0 premature=0/0 after=2/0 expose=1/0 visible=1
BXSUMMARY passed=23 failed=0 observational_input=yes
```

The real IceWM observer now finds two painted title bars and two painted close
buttons. Every pixel is nonzero and each drawable contains multiple colors:

```text
BXTEST PASS icewm-decoration-pixels painted=4/4
BXSUMMARY icewm passed=4 failed=0
```

The Android screenshot in `evidence/icewm-managed-windows.png` shows both
titles and their system/minimize/maximize/close buttons. The run used the
ordinary app UID with no Frida, proot, Termux, or root requirement and emitted
no unsupported opcode or X11 request error.
