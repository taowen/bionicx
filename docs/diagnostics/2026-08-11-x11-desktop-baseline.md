# Desktop X11 extension baseline

## Purpose

Real Chromium and Qt desktop programs do more than open a core X11 window. To
make those dependencies testable independently of either application,
`x11-desktop-probe` is a genuine AArch64 glibc client linked against
`libXrender`, `libXfixes`, `libXrandr`, `libXi`, `libXext` and `libX11`.

The probe does not count extension advertisement as success. Each strict check
negotiates a version and performs one stateful operation:

- Render creates a Picture, fills a rectangle, and reads back a nonzero pixel;
- XFixes creates, fetches, validates, and destroys a Region;
- RandR requires at least one CRTC, output, and mode;
- XInput2 enumerates at least the master pointer and keyboard;
- XKB retrieves the core keyboard map.

MIT-SHM remains a reported capability rather than a strict check. Advertising
it before Android-backed shared memory and lifetime semantics are correct would
cause desktop clients to select an unsafe path.

## x300 baseline

On Android 14 device `01408BH601027129`, the ordinary untraced executor loaded
the complete glibc dependency closure and connected to the built-in X server.
The server did not advertise any of the five strict extensions:

```text
BXTEST FAIL xrender      extension-missing
BXTEST FAIL xfixes       extension-missing
BXTEST FAIL randr        extension-missing
BXTEST FAIL xinput2      extension-missing
BXTEST FAIL xkeyboard    extension-missing
BXCAP mit-shm unavailable version=0.0 pixmaps=0
BXSUMMARY desktop-x11 passed=0 failed=5 xerrors=0
```

The zero X-error count and clean process completion distinguish missing server
features from a loader, ABI, socket, or malformed-protocol failure. The raw
launch and result log is retained in
`evidence/x11-desktop-probe-baseline.log`.

## Implementation order

Render comes first because toolkit composition and Chromium's software drawing
paths depend on Pictures and formats. XFixes is next for regions and selection
notifications, followed by RandR, XKB, and XInput2. Each extension is advertised
only once the stateful request sequence in this probe passes without X errors.
