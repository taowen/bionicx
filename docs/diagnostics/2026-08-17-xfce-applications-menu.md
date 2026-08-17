# XFCE Applications menu

XTEST clicking the panel bar at `(36,13)` or `(12,13)` does not map a
new menu. `XQueryPointer` hits the xfwm4 dock frame (`2640x27`),
`XGrabPointer` is free, and a framed-dock ButtonPress probe passes
(`reparent-x11-probe` `dock-click`). The compositor overlay is not the
hit target.

A `116x53` xfce4-panel override-redirect window sits at `-4+26` before
the click (tooltip-sized). It is not a grab owner and can stay black.

The mapped 185x324 menu has only a 1x1 child. Isolated `GarconGtkMenu`
under a stub compositor paints `94%` light (`gtk-garcon-paint` 131x312).
The session menu used to read back `pixel=0x000000 nonzero=7173
light=1236` because cairo will not send CompositeTrapezoids unless
Render is at least 0.4; glyphs used CompositeGlyphs (available at 0.0)
and the rounded Adwaita body fell back to an image path that never
landed. After advertising 0.4 and rasterizing traps:

```text
BXINFO menu-paint 0x100013d 185x324+-6+22 depth=32 pixel=0xffffff nonzero=52588 light=49616 bright=0xffffff
```

`xfce4-popup-applicationsmenu` maps a real menu:

```text
BXTEST PASS session-applications-menu menu 185x324
BXSUMMARY xfce-session-accept passed=12 failed=0
```

The session profile sets `XDG_MENU_PREFIX=xfce-` so garcon loads
`/etc/xdg/menus/xfce-applications.menu`.

The Applications plugin opens on `button-press-event` with button 1 and
no Control. `gtk-menu-probe` `gtk-press-menu` passes that path under a
framed stub WM (`fired=1 type=4 button=1 state=0x0`). An XTEST click at
`(12,13)` on the panel bar (`0x1000005` 2640x27+0+0) still does not map
a menu when `xfwm4` is the WM. QueryPointer says the hit is correct:
root child is the xfwm4 frame (`0x8003ff` 2640x27+0+0), frame child is
the panel client, and the panel has no child at that point. Isolated
`dock-client-replay` and overlay click-through pass. `AllowEvents`
SyncPointer now thaws frozen pointer events without dropping the grab
(GTK menus SyncPointer while still owning the pointer). A synchronous
passive `GrabButton` now activates before XI2 and reports that press
only on the grab window, so xfwm4 sees `event.window == c->window` and
Replays (`dock-xi-replay` `pre_frame=0`). Implicit and `XIGrabDevice`
grabs still broadcast XI2 so `gtk-menu-click` stays 13/13. An XTEST
click at `(12,13)` in the live session still does not map a menu. The
plugin has no child X window; a 133x26 child at `x=2507` is the clock.
