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
as success. Official `xXIGroupInfo` is four CARD8s, so DeviceEvent is
80 bytes plus the button word (84, `length=13`) and Enter/Leave is 76
(`length=11`). Treating group as four CARD32s emitted 96/88-byte
events and left `buttons.mask` on zeros; `mask_len >= 4` still passed.
The probe now requires button 1 in that mask and ignores core presses.

The live Applications click still writes the press to xfwm4 and still
never AllowEvents. A failed client write now keeps the unsent tail
instead of dropping it. Frozen grabs no longer emit XI2 ButtonRelease.

`grab-press-io` during the 12 accept tests is not xfwm4: it is
Mousepad `0x2400000` and `0x2800000`. After the Applications click
xfwm4 is still reading the socket:

```text
grab-trace … client=0x800000 … last=opcode=25 data=1 seq=9405
grab-xi sent type=4 … grab-xi sent type=5
grab-client-req client=0x800000 opcode=25 data=1 seq=9406…
grab-client-req client=0x800000 opcode=3 data=1 seq=9410
grab-client-req client=0x800000 opcode=14 data=0 seq=9411
click-menu none grab=0->1 no popup
```

opcode 25 is `SendEvent`. While the grab stays frozen:

```text
grab-send-event client=0x800000 dest=0x1000005 type=22 send=false
                mask=131072 propagate=true
grab-send-event client=0x1000000 dest=0x4 type=33 send=false
                mask=1572864 propagate=false
```

type 22 is ConfigureNotify to the panel (`StructureNotifyMask`,
propagate=true — xfwm4 `clientConfigure`, not `handleButtonPress`).
type 33 is a panel ClientMessage to root. opcode 3/14 are
GetWindowAttributes/GetGeometry. No core 35 or XI AllowEvents in the
1.5s wait.

Isolated `gtk-gdk-grab` 8/8 shows GDK does dequeue a sync
`XIGrabButton` press through `gdk_window_add_filter(NULL)` on its own
window, a peer window, a redirected peer, and after hover motion. Every
case is `core=0 send=0 xi=1 allow=1`: GDK's `XFilterEvent` eats both
real and SendEvent-bit core `ButtonPress` before the filter. The
pointer path is the XI2 cookie.

A passive sync-grab press now sets that SendEvent bit and, after the
XI2 press, writes a `PropertyNotify(WM_NAME)` marker. Live Applications
click:

```text
grab-press sent=0x1000005 client=0x800000 mask=204 send=1
grab-xi sent type=4 client=0x800000 target=0x1000005 cookie=-109
grab-mark sent client=0x800000 window=0x1000005 atom=39 WM_NAME
grab-client-req client=0x800000 opcode=20 data=0 seq=…
click-menu none grab=0->1 no popup
```

opcode 20 is `GetProperty`. xfwm4's `handlePropertyNotify` calls
`clientUpdateName` for `WM_NAME`, so the grabber dequeued past both
the send_event core press and the XI2 press. It never AllowEvents, so
`handleButtonPress` did not run. The press is not stuck behind
ConfigureNotify or a frame-clock pause.

`XFilterEvent` returning True is a silent `continue` in
`gdkeventsource.c`. There is no GDK log for that drop. Seed
`libgdk-3.so.0` is built without `G_ENABLE_DEBUG`, so
`GDK_DEBUG=events` is also a no-op (`gdk-ev-summary new_lines=0`;
the only stderr is an AT-SPI warning). xfwm4 `TRACE` is compiled out.
The seed `xfwm4` is not linked to `libXi`, so its filter has no XI2
button branch even when a cookie reaches GDK.

Isolated `gtk-gdk-grab` now prints every event that survives
`XFilterEvent` and hits `gdk_window_add_filter`:

```text
BXINFO gdk-filter type=35 send=0 ext=147 data=1 evtype=4
```

`type=4` core `ButtonPress` never appears (`core=0 send=0`). Only
XI2 GenericEvent (`type=35`, cookie present, `evtype=4`) reaches the
filter. A simple `XOpenIM` does not change that
(`gdk-grab-im … xi=1 allow=1`).

A core-only grabber that matches the seed WM (`XGrabButton` only, no
`XIQueryVersion` / `XIGrabButton`, `GDK_CORE_DEVICE_EVENTS=1`) is:

```text
BXINFO gdk-grab-core core=0 send=0 xi=0 allow=0
```

That grab never thaws. After delivering the exclusive sync press, the
server now replays to the owner when the grab client never sent
`XIGrabButton`. Live Applications click:

```text
grab-core-replay client=0x800000
grab-press sent=0x1000005 client=0x1000000 mask=6529151 send=0
click-menu opened grab=0->1 menu 185x324
```

`session-applications-menu` still maps 185x324 before the click. Do
not add a 13th xfce accept click.

After that replay the leftover Applications tooltip is not mapped:

```text
find-Xfce4-panel 0x1000416 parent=0x4 116x53+-4+26 map=0
pre-click-tip none
post-click-tip none
```

The earlier black 116x53 box was the hover tooltip left up while the
sync grab stayed frozen. It is not a separate paint bug.

Reopened Mousepad chrome paints (title `Untitled 1`, Adwaita menubar).
The editor pixmap is 100% `0xfcfcfc` because the buffer is empty. An
XTEST `bx` after `_NET_ACTIVE_WINDOW` still leaves the editor crop
without letter ink (about 17 mid pixels, caret-sized).

Isolated `gtk-textview` sets `BxGlyphs` in-process. The widget window
has paper+ink (`ink=182`). The TEXT child stays `pixel=0x000000` until
`XClearArea` (`tv-clear` then `ink=182`). `CWBackPixel` no longer
fills the pixmap (`change-background-keeps-pixels`); GDK's
`tmp_reset_bg` after Map/Configure must not wipe paint.
`XSelectInput(Exposure)` on a viewable window generates Expose
(`select-exposure-mapped`). `PictStandardRGB24` is advertised for
cairo's opaque surfaces. The TEXT child still needs a later Expose
before it paints; a compositor that presents that child still shows a
blank editor until then.
