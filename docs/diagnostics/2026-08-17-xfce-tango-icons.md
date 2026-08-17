# XFCE desktop / Applications icons

The 6s session screenshot has a cyan Applications mouse at the panel
origin and a blue Home house on the desktop. Those names live in
Adwaita / hicolor. Debian XFCE defaults `Net/IconThemeName=Tango`, but
the seed `Tango` directory is only Geany's `geany-save-all` and has no
`index.theme`. GTK 3 still finds Adwaita as its fallback theme, so a
Tango setting is not enough to hide `user-home`.

The session home still pins `IconThemeName=Adwaita` so xfsettingsd
publishes a theme that is actually installed, instead of depending on
that fallback.

## Controlled client

`examples/gtk-desktop-icon-probe` forces Adwaita and loads `user-home`
(blue), `drive-harddisk`, and `org.xfce.panel.applicationsmenu` (cyan).
