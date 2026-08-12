# Debian trixie GIMP, Inkscape and VLC

## Scope and result

The fixed Debian 13 snapshot `20260811T000000Z` now installs these unmodified
ARM64 packages into the same apt/dpkg image as Chrome, WPS, IceWM, xterm and the
XFCE cohort:

- GIMP `3.0.4-3+deb13u9`;
- Inkscape `1.4-6`;
- VLC `3.0.23-0+deb13u1`.

The resulting rootfs contains 563 packages and its GLIBC symbol-floor check
covers 2,159 ELF objects. All final tests ran untraced as Android `u0_a194` on
x300 serial `01408BH601027129`, without root, PRoot, Termux or Frida.

GIMP decoded the generated PNG through its packaged file plug-in, then an
Android swipe painted a visible stroke on the canvas. Inkscape parsed and
rendered the generated SVG with gradients, clipping, text and multiple shapes.
VLC looped a deterministic 320x180, 30 fps raw I420 animation. Its screenshots
two seconds apart had different SHA-256 hashes and visibly different frames.

Evidence:

- `evidence/bionicx-trixie-gimp-paint.png`
- `evidence/bionicx-trixie-inkscape-svg.png`
- `evidence/bionicx-trixie-vlc-i420.png`

## Runtime gaps exposed

Debian packages contain more absolute FHS assumptions than the main executable.
GIMP launches packaged plug-in ELFs, so only patching a hand-picked entry point
left child programs with `/lib/ld-linux-aarch64.so.1`. The rootfs builder now
patches PT_INTERP on every executable ELF. It also converts internally
resolvable absolute symlinks to relative links, preserving dpkg alternatives
without letting them escape to Android's filesystem.

VLC's `libxcb_x11_plugin.so` carries an absolute
`/usr/lib/aarch64-linux-gnu/vlc` RUNPATH. Its sibling
`libvlc_xcb_events.so.0` therefore could not be found even though dpkg had
installed it. The builder now relocates absolute `/usr`, `/lib` and `/lib64`
DT_RPATH/DT_RUNPATH components to the fixed app-private rootfs. The final VLC
profile consequently uses only the two standard shared library directories;
there is no VLC-specific dependency-path workaround.

## X11 gaps exposed

VLC also found two server-side robustness problems. First, its short WM_HINTS
property triggered an out-of-bounds Java `ByteBuffer` read in window
classification. Property integer, long and byte access is now bounds checked;
a missing optional field reads as zero.

Second, libxcb marks a connection unusable if a request is attempted against an
absent extension. VLC queried absent MIT-SHM, noticed that it was unavailable,
but then reused the poisoned video connection. BionicX now makes MIT-SHM
discoverable and returns standard `BadImplementation` from QueryVersion when no
SHM backend exists. Qt and VLC retain the connection and VLC correctly falls
back to ordinary XPutImage. This is deliberately not a claim that MIT-SHM
transport exists: shared-memory attach and image transfer still require a real
Android-compatible backend.

The final cold run used no application library exception and rendered moving
video. Expected diagnostics remain for disabled PulseAudio/D-Bus, the MIT-SHM
QueryVersion fallback, and an optional XFixes request; none terminated or
blanked the client.
