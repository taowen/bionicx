# Popular creative and media application cohort

This cohort adds three real Debian 13 trixie ARM64 applications to the same
pinned apt/dpkg runtime used by Chrome, WPS, IceWM, xterm and the XFCE cohort:

- GIMP 3 exercises a large GTK image-editing application and plugin runtime.
- Inkscape exercises SVG parsing, Cairo rendering and a complex multi-pane UI.
- VLC exercises demux, decode, timing and X11 video presentation.

The bundle generates deterministic PNG, SVG, YUV4MPEG2 and raw I420 fixtures
without downloading per-application dependency trees. VLC uses the raw I420
file with explicit geometry, chroma and frame rate so its demux and display
path is deterministic:

```sh
examples/popular-apps/build-bundle.sh
tools/install-profile.sh --profile profiles/gimp.json \
  --app-root build/popular-apps-bundle/app \
  --runtime-root build/popular-apps-bundle/rootfs
```

Use `profiles/inkscape.json` or `profiles/vlc.json` for the other applications.
An accepted integration must launch untraced under the ordinary Android app
UID and complete a visible application-specific workflow.

All three applications pass on x300. GIMP decodes the PNG through its packaged
plug-in and accepts a real painted stroke; Inkscape parses and renders the SVG;
VLC loops the 90-frame I420 animation through its XCB/XPutImage fallback. They
run as `u0_a194`, without PRoot, root, Frida or application-specific library
copies. See `docs/diagnostics/2026-08-13-trixie-popular-apps.md`.
