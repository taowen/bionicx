# Read-only RandR 1.3 topology

## Requirement

Desktop toolkits query RandR even on a fixed embedded display. The controlled
client calls `XRRGetScreenResourcesCurrent`; accepting the extension version
alone is insufficient because libXrandr then parses variable-length CRTC,
output, mode, and name arrays.

## Single-display model

`XRandRExtension` exposes BionicX's Android-backed X screen as a stable,
read-only RandR 1.3 topology:

- one CRTC;
- one output;
- one mode named `BionicX-0`;
- mode width and height taken directly from `XServer.screenInfo`;
- both `GetScreenResources` 1.2 and `GetScreenResourcesCurrent` 1.3 use the same
  internally consistent snapshot.

The extension does not yet claim output-info, CRTC-info, reconfiguration,
primary-output, monitor, or event-delivery support. Those operations require
stronger controlled clients before they are enabled.

## Device proof

The AArch64 glibc/libXrandr probe additionally checks that the decoded mode
matches core Xlib's `DisplayWidth` and `DisplayHeight` and has a non-empty name:

```text
BXTEST PASS randr version=1.3 crtcs=1 outputs=1 mode=1920x1080 name=BionicX-0
BXSUMMARY desktop-x11 passed=3 failed=2 xerrors=0
```

The untraced x300 result is retained in
`evidence/x11-desktop-probe-randr.log`.
