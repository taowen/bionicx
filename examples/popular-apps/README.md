# Popular creative and media application cohort

This cohort adds six real Debian 13 trixie ARM64 applications to the same
pinned apt/dpkg runtime used by Chrome, WPS, IceWM, xterm and the XFCE cohort:

- GIMP 3 exercises a large GTK image-editing application and plugin runtime.
- Inkscape exercises SVG parsing, Cairo rendering and a complex multi-pane UI.
- VLC exercises demux, decode, timing and X11 video presentation.
- Geany exercises GTK editing, syntax highlighting and file persistence.
- FileZilla exercises wxWidgets, TLS/network configuration and a dense UI.
- Thunderbird exercises Mozilla's large GTK/X11 and multi-process runtime.

The bundle generates deterministic PNG, SVG, YUV4MPEG2, raw I420, PCM WAV and
AVI fixtures without downloading per-application dependency trees. The AVI is
muxed directly in Python from 90 I420 frames and a 48 kHz stereo tone. VLC
therefore exercises one real AVI demux, raw-video decoder, PCM decoder, X11
video output and PulseAudio-to-AAudio path in the same process:

```sh
examples/popular-apps/build-bundle.sh
tools/install-profile.sh --profile profiles/gimp.json \
  --app-root build/popular-apps-bundle/app \
  --runtime-root build/popular-apps-bundle/rootfs
```

Use `profiles/inkscape.json`, `profiles/vlc.json`, `profiles/geany.json`,
`profiles/filezilla.json` or `profiles/thunderbird.json` for the other
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
