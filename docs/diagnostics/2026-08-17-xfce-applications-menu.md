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

The remaining live gap was why the panel frame is gone from the
mapped root stack after the session raises windows, while the
compositor still paints it. `xfce4-panel` exits after a fatal GDK X
error:

```text
The error was 'BadImplementation (server does not implement operation)'.
  (Details: serial 6575 error_code 17 request_code 151 (RENDER) minor_code 25)
```

RENDER minor 25 is `CompositeGlyphs32`. Pango draws panel labels with
32-bit glyph ids. The server already rasterized CompositeGlyphs8/16
and threw `BadImplementation` for 32. GDK treats that as fatal, so
window `0x1000005` is destroyed (`saved-bar … gone`) and the click
hits xfdesktop. CompositeGlyphs32 now rasterizes those ids. After
that the panel stays mapped (`find-Xfce4-panel 0x1000005`) and
`pre-click-ptr` is the dock frame, not Xfdesktop. The XTEST click
still reports `click-menu none grab=0->1` and a black 116x53
Applications tooltip can remain. The press activates a synchronous
grab on the panel client (`ptr-press … grab=0x1000005 sync=true`)
and the grabber never core-`AllowEvents`. GDK thaws XI2 sync grabs
with `XIAllowEvents`; that request was accepted as a no-op, so the
pointer stayed frozen and the plugin never saw button 1.

After honoring `XIAllowEvents`, one `grab-trace` line names the rest:

```text
grab-add w=0x1000005 client=0x800000 owner=0x1000000 button=0 mods=0x8000 sync=true mask=204
grab-press sent=0x1000005 client=0x800000 mask=204
grab-trace btn=1 point=0x1000005 grab=0x1000005 client=0x800000 origin=0x1000000
          sync=true ownerEvents=false enabled=true grabs=…/c=0x800000/b=0/m=0x8000
click-menu none grab=0->1 no popup
```

The press is written to xfwm4 (`0x800000`) on the panel client window.
No core or XI `AllowEvents` follows. Isolated `gtk-xi-allow-menu` still
passes because that stub WM is raw Xlib and treats core `ButtonPress`
as enough. Live xfwm4 is GDK 3.24 (`sources/gtk-3.24`):

- `gdk_window_add_filter(NULL)` does run on every `XNextEvent`.
- After the filter, `gdk_x11_device_manager_xi2_translate_core_event`
  ignores real core button events and only accepts `send_event` ones.
  GDK expects the pointer path to be an XI2 cookie (`cookie.data != NULL`).
- xfwm4's own filter translates core `ButtonPress` unconditionally, and
  `handleButtonPress` always `XAllowEvents`. Missing opcode 35 means
  GDK never dequeued that press, or the XI2 cookie was null so the
  XI2 branch became `XFWM_EVENT_XEVENT`.

`gtk-xi-allow-menu` used to treat a core `ButtonPress` on the stub WM
as success, so an 84-byte XI2 `DeviceEvent` (group short, button mask
over those fields) still passed. GDK reads `buttons.mask` after
`XGetEventData`; that cookie was empty. The probe now requires
`buttons.mask_len >= 4` and ignores core presses for that case. After
the 96-byte layout, vivo `gtk-xi-allow-menu` reports
`cookie=1 detail=1 mask_len=4`.

The live Applications click still writes the press to xfwm4 and still
never AllowEvents. XI2 Enter/Leave used the same short group encoding,
so `XGetEventData` could over-read into the following ButtonPress.
`xi2-enter-cookie` now requires `buttons.mask_len >= 4`. A failed
client write now keeps the unsent tail instead of dropping it. The
compositor connection also logged `grab-press-io Failed to send data`
during the 12 accept tests. Do not add a 13th xfce accept click.
