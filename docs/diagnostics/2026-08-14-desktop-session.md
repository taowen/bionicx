# IceWM desktop session with two package apps

Device `01408BH601027129`, seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.

`profiles/desktop-session.json` now requests `dbus`, `pulseaudio`, `cups`
and `vulkan` together. The activity logged all four host services, then
`examples/desktop-session/desktop-session.c --accept` launched
package-installed IceWM, xterm and mousepad from the shared rootfs
(no per-app `libc.so.6`).

```text
BXTEST PASS desktop-session-launch icewm=10234 app1=10251 app2=10256
BXTEST PASS session-two-mapped xterm=0x800014 mousepad=0xc00003
BXTEST PASS session-switch-xterm focus=xterm
BXTEST PASS session-switch-mousepad focus=mousepad
BXTEST PASS session-resize-xterm 644x340 -> 740x382
BXTEST PASS session-close-mousepad client withdrawn
BXTEST PASS session-reopen-mousepad pid=10287 alive=1
BXSUMMARY desktop-session-accept passed=7 failed=0
```

The 4 s compositor frame shows both windows painted on the IceWM
taskbar. `xdg-open` remains the shared Debian opener on `PATH`; this
session does not add a per-app dispatcher.

Rerun `examples/desktop-session/install-and-run.sh` to recapture under `build/evidence/`.
