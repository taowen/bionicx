# GTK menu ConfigureWindow under GrabServer

`MapWindow` already applies for a `GrabServer` owner while a WM holds
`SubstructureRedirect`. `ConfigureWindow` still sent only
`ConfigureRequest`, so the owner waited for a `ConfigureNotify` the WM
could not send until `UngrabServer`. GTK menus `GrabServer` then resize
the popup.

## Controlled client

`examples/map-request-x11-probe` `XResizeWindow`s under `GrabServer`.
Before the grab-owner configure path:

```text
BXTEST FAIL grab-owner-configure ConfigureRequest blocked by SubstructureRedirect
```

After applying the resize and still delivering `ConfigureRequest`:

```text
BXTEST PASS grab-owner-configure 240x90 under GrabServer
BXSUMMARY map-request-x11 passed=7 failed=0
```

`examples/gtk-menu-probe` is the same popup without a WM:
`gtk_menu_popup_at_widget` maps a `GDK_WINDOW_TEMP` of at least 40x16.
