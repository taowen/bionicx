# IceWM session launching two package apps

`profiles/desktop-session.json` starts package-installed `icewm`, `xterm` and
`mousepad` under one profile with `hostServices: ["dbus", "cups"]`. PulseAudio
and Vulkan are left for the later GPU/audio gates.

On `01408BH601027129` the untraced session printed
`BXTEST PASS desktop-session-launch` with three live pids. The screenshot
shows the IceWM taskbar. `xterm` logged a missing SHAPE/`X_ImageText8`
implementation; `mousepad` logged missing GTK composite-template resources.
Those app surfaces are not claimed complete.

A CUPS job was submitted to the existing `bionicx-test` destination while the
session cupsd was up: `lp` returned `bionicx-test-2` and `lpstat -W completed`
listed it. WPS is not installed on this seed yet.
