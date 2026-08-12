# IceWM multi-window reparenting

The first desktop milestone deliberately uses the unmodified Debian trixie
ARM64 IceWM 3.7.4 binary as a controlled real-world window manager. It runs as
a glibc X11 client under the ordinary Android app UID and manages two separate
glibc clients in the same full-screen BionicX surface.

IceWM exposed three server defects that single top-level applications did not:

1. Mapped `InputOnly` windows entered `GLRenderer`'s drawable list even though
   they have no drawable, causing an Android `NullPointerException`.
2. `ReparentWindow` discarded the request's x/y coordinates and emitted no
   `ReparentNotify` events.
3. `MapWindow` and `ConfigureWindow` redirected a request whenever the parent
   had `SubstructureRedirect`, even when the request came from that redirect
   owner. IceWM therefore received its own request again instead of mapping the
   application inside its container.

The renderer now traverses but does not draw `InputOnly` windows. Reparenting
applies the coordinates, refreshes scene geometry and sends standard event 21
to structure/substructure listeners. Map/configure redirect decisions now
exclude listeners owned by the requesting X client.

On x300 `01408BH601027129`, IceWM then selected the root redirect mask, created
two frames and completed both client lifecycles:

```
BXTEST PASS icewm-manager-start
BXICEWM client title=BionicX Workspace A mapped=1 configured=1 size=620x310
BXICEWM client title=BionicX Workspace B mapped=1 configured=1 size=620x310
BXTEST PASS icewm-two-clients first=1 second=1
BXSUMMARY icewm passed=2 failed=0
icewm-probe exited with 0
```

The existing core X11 suite also remains green at 18/18. See
`evidence/icewm-managed-windows.png` and `evidence/icewm-managed-windows.log`.

IceWM's taskbar remains disabled for this first milestone. Enabling it still
exercises unresolved font/icon and taskbar paths; a complete panel, menu and
desktop session are explicitly outside this increment.
