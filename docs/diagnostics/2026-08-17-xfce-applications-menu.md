# XFCE Applications menu

XTEST clicking the panel bar at `(36,13)` or `(12,13)` does not map a
new menu. `XQueryPointer` hits the xfwm4 dock frame (`2640x27`),
`XGrabPointer` is free, and a framed-dock ButtonPress probe passes
(`reparent-x11-probe` `dock-click`). The compositor overlay is not the
hit target.

A `116x53` xfce4-panel override-redirect window sits at `-4+26` before
the click (tooltip-sized). It is not a grab owner.

`xfce4-popup-applicationsmenu` maps a real menu:

```text
BXINFO applications-popup pid=11953 before_or=7
BXTEST PASS session-applications-menu menu 185x324
BXSUMMARY xfce-session-accept passed=12 failed=0
```

The session profile sets `XDG_MENU_PREFIX=xfce-` so garcon loads
`/etc/xdg/menus/xfce-applications.menu`. Opening the menu by clicking
the Applications plugin remains pending.
