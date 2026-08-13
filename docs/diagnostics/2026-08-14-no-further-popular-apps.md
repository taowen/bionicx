# No further popular ARM64 applications

The shared seed already exercises the contracts a new Debian app would
need:

- GTK3 file/document/mail: Thunar, Mousepad, Ristretto, Evince, GIMP,
  Geany, FileZilla, Thunderbird
- Qt5 painting and secrets: Krita, KeePassXC, WPS
- Qt6 network: qBittorrent
- Mozilla multi-process: Firefox ESR, Thunderbird
- GLX/OpenGL (Gladio) and Vulkan (Vortek/Chrome)
- PulseAudio, session D-Bus, CUPS, IceWM desktop session

Candidates that were considered and not added:

- Transmission, Deluge: same BitTorrent/Qt path as qBittorrent
- Audacious, mpv: same decode/Pulse/X11 path as VLC
- HexChat, Pidgin: same GTK/network path as Thunderbird/FileZilla
- Gedit, Pluma: same GTK editor path as Mousepad/Geany
- Chromium: Chrome already covers ANGLE Vulkan
- Blender, Kdenlive: large per-app plugin/Python trees, not a new
  shared runtime contract

Adding any of those would grow the declared set without extending the
shared rootfs, host services, or probe surface. The cohort stays the
names in `packages/trixie-popular.txt` plus the hash-pinned Chrome,
WPS, `libwebp6` and `libtiff5` entries.
