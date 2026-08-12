# Cross-client X11 passive pointer-grab probe

This genuine AArch64 glibc/libX11 client installs an `AnyButton` and
`AnyModifier` passive grab on the root window from one connection while a peer
window receives the physical Android tap. The grab uses a real cursor-font
`XCreateFontCursor` resource and renderer override. It checks that press and release are
routed only to the grab owner. A second grab with `owner_events=True` verifies
normal delivery to a window selected by that same connection. It then calls
`UngrabButton` and verifies that the final tap follows normal pointer routing
to the peer. A competing registration from the peer must receive `BadAccess`.

```sh
ANDROID_SERIAL=<serial> examples/pointer-grab-x11-probe/install-and-run.sh
```

Success requires five strict checks, zero unexpected X errors, and a
normal process exit. The taps are server input, not synthetic X events.
