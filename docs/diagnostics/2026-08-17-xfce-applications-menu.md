# XFCE Applications menu

XTEST clicking the panel bar at `(36,13)` or `(12,13)` does not map a
new menu. `xfce4-popup-applicationsmenu` still maps a 185x324 Adwaita
menu (`session-applications-menu`). Isolated GTK press and framed
sync-grab replay stay 14/14. Do not add a 13th xfce accept click.

After the 12 accept tests the saved click hits xfdesktop and leaves a
grab:

```text
BXINFO pre-click-ptr child=0x8005dc 2640x1216+0+0 child_class=Xfdesktop
BXINFO click-menu none grab=0->1 no popup
```

The panel is painted, so the button looks clickable. A mapped-only
root stack dump at that point has no 2640x27 dock frame — only the
overlay, xfwm4 sidewalks, Thunar/Mousepad frames, and xfdesktop.
Raising docks below the overlay is not Xorg behavior and does not
fix this.

xfwm4's compositor sets the overlay Bounding and Input shapes to
empty so the overlay tree is a hole and sibling clients keep the
pointer. This server used to treat `SetWindowShapeRegion(region=None)`
as "remove the shape" for Input only, and ignored Bounding. Pointer
pick also folded Input into "enter this window", so an overlay with
a full-screen output child could steal hits. Pick now walks children
inside the Bounding box and applies Input only to the window itself.
`SetWindowShapeRegion(None)` is an empty region for every shape kind.
`overlay-click-dock` restacks the dock above a desktop under that
hole and still hits the dock.

The remaining live gap is why the panel frame is gone from the
mapped root stack after the session raises windows, while the
compositor still paints it.
