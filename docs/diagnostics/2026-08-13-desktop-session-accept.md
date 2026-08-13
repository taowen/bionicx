# IceWM two-app switch, resize, close and reopen

`profiles/desktop-session.json` now passes `--accept`. After IceWM maps
package `xterm` and `mousepad`, the session binary (a genuine glibc/libX11
client) finds those windows by `WM_CLASS`, activates each, resizes xterm,
withdraws mousepad and starts it again with `--disable-server`.

On `01408BH601027129`:

```text
BXTEST PASS desktop-session-launch icewm=23204 app1=23220 app2=23224
BXTEST PASS session-display :0
BXTEST PASS session-two-mapped xterm=0x800014 mousepad=0xc00003
BXTEST PASS session-switch-xterm focus=xterm
BXTEST PASS session-switch-mousepad focus=mousepad
BXTEST PASS session-resize-xterm 644x340 -> 740x382
BXTEST PASS session-close-mousepad client withdrawn
BXTEST PASS session-reopen-mousepad pid=23254 alive=1
BXSUMMARY desktop-session-accept passed=7 failed=0
```

IceWM may leave an empty frame after the client is withdrawn; the accept
checks the client window and the new process, not that leftover frame.
Seed `ed998c09…` was not replaced. Host
`tests/test-desktop-session-profile.sh` requires the four accept names.
