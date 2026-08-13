# Popular creative and media application cohort

This cohort adds nine real Debian 13 trixie ARM64 applications to the same
pinned apt/dpkg runtime used by Chrome, WPS, IceWM, xterm and the XFCE cohort:

- GIMP 3 exercises a large GTK image-editing application and plugin runtime.
- Inkscape exercises SVG parsing, Cairo rendering and a complex multi-pane UI.
- VLC exercises demux, decode, timing and X11 video presentation.
- Geany exercises GTK editing, syntax highlighting and file persistence.
- FileZilla exercises wxWidgets, TLS/network configuration and a dense UI.
- Thunderbird exercises Mozilla's large GTK/X11 and multi-process runtime.
- Krita exercises a large Qt painting application, image codecs and durable
  image editing.
- qBittorrent exercises a modern Qt network client and persistent transfer
  metadata.
- KeePassXC exercises encrypted database creation, unlock and clipboard use.

The fixture builder generates deterministic PNG, SVG, YUV4MPEG2, raw I420, PCM
WAV, AVI and torrent fixtures. It never builds or copies a rootfs; install the
declared packages into the phone's shared rootfs first. The AVI is
muxed directly in Python from 90 I420 frames and a 48 kHz stereo tone. VLC
therefore exercises one real AVI demux, raw-video decoder, PCM decoder, X11
video output and PulseAudio-to-AAudio path in the same process:

```sh
examples/popular-apps/build-bundle.sh
tools/bxapt --serial <serial> set packages/trixie-popular.txt
ANDROID_SERIAL=<serial> examples/popular-apps/install-and-run.sh qbittorrent
```

Install with `--app-root` only. A fixture bundle must not replace the
shared device seed.

Use `profiles/inkscape.json`, `profiles/vlc.json`, `profiles/geany.json`,
`profiles/filezilla.json`, `profiles/thunderbird.json`, `profiles/krita.json`,
`profiles/qbittorrent.json` or `profiles/keepassxc.json` for the other
applications.
An accepted integration must launch untraced under the ordinary Android app
UID and complete a visible application-specific workflow.

All six applications pass on x300. GIMP decodes the PNG through its packaged
plug-in and accepts a real painted stroke; Inkscape parses and renders the SVG;
VLC loops the 90-frame audiovisual AVI through its XCB/XPutImage and Android
AAudio paths; Geany edits and saves C source; FileZilla opens its Site Manager;
and Thunderbird accepts input in its account workflow with four Mozilla child
processes alive. They run as `u0_a194`, without PRoot, root, Frida or
application-specific library copies. See
`docs/diagnostics/2026-08-13-trixie-popular-apps.md` and
`docs/diagnostics/2026-08-13-trixie-more-popular-apps.md`.
