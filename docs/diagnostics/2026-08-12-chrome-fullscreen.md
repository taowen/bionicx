# Chrome full-screen X11 window

## Symptom

The embedded X server advertised and rendered a 1920x1080 root window, but
Chrome's default initial top-level window was only about 955 pixels wide. The
unused right side therefore stayed black even though coordinates and Android
surface scaling were correct.

## Correction and result

The Chrome profile now asks for a deterministic top-level geometry with
`--window-position=0,0`, `--window-size=1920,1080`, and `--start-maximized`.
This is profile policy rather than a renderer special case and leaves arbitrary
X11 clients free to choose smaller windows.

On x300 the next cold, untraced launch covered the complete 1920x1080 Android
surface. Android-injected keyboard input navigated to Example Domain, which
loaded and rendered across the full X11 window. There was no black edge and no
fatal, NSS, child FD-ownership, or network-service restart message. The three
remaining XFixes selection-mask errors are retained in the log as the next
independent server capability to fix.

See `evidence/chrome-fullscreen-network.png` and
`evidence/chrome-fullscreen-network.log`.
