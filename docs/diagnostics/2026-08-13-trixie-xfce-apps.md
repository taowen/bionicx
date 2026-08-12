# Debian Thunar, Mousepad and Ristretto integration cohort

## Scope

The pinned Debian 13 snapshot `20260811T000000Z` now installs these genuine
ARM64 packages into the shared Chrome/WPS/IceWM/xterm rootfs:

```text
mousepad  0.6.3-1
ristretto 0.13.3-1
thunar    4.20.2-1+deb13u1
```

The single apt/dpkg image contains 316 packages and has content ID
`45db8ac59a20e8d680f1148a4699e2e2b4c14ca308857656b1bf0e158a05244a`.
There are no per-application dependency copies. All final launches use the
ordinary `io.taowen.bx` app UID and `bionicx-exec` reports untraced processes.

## X11 failures and fixes

The controlled GTK cohort exposed three standard protocol gaps in order:

1. Thunar aborted on Render Composite with `BadValue.data=13`. Operation 13 is
   `PictOpSaturate`; the server now accepts it and implements its Porter-Duff
   source contribution for depth-8 and depth-32 drawables.
2. Opening a GTK menu issued XI2 `XIGrabDevice` for both master pointer 2 and
   master keyboard 3. XI grab/ungrab now reuses the existing core grab state,
   validates asynchronous modes and cursor/window resources, and returns the
   protocol grab status reply.
3. Clicking Mousepad's editor issued XI2 `XIChangeCursor`. The request now
   updates the target window's inherited cursor and validates the cursor and
   master-pointer selector.

XI pointer events now account for an active pointer grab before performing
normal selected-event propagation. The remaining Thunar menu-item activation
gap shows that grab-time release/child semantics still need a focused probe;
the application no longer receives a fatal X error.

## Real application results

- **Ristretto: pass.** The bundle deterministically generates blue and orange
  960x540 PNG fixtures. Ristretto decodes both, creates thumbnails, and a real
  pointer click selects the orange image. The status bar reports
  `bionicx-orange.png`, `960 x 540`, `7.7 kB`, and `51.9%`.
- **Mousepad: partial.** It opens and renders the real bundled note through
  GtkSourceView. Injected editing and save/reopen are not accepted yet because
  keyboard focus/rendering after input still needs diagnosis.
- **Thunar: partial.** It maps its file-manager window and can open a stable
  GTK File menu through XI2 grabs. Selecting Create Folder does not yet
  activate, so no file-operation acceptance is claimed.

Evidence:

- `evidence/bionicx-trixie-ristretto.png`
- `evidence/bionicx-trixie-mousepad.png`
- `evidence/bionicx-trixie-thunar-menu.png`

The missing Xfconf/session-bus machine ID and libmagic's absolute
`/usr/share/misc/magic` lookup are recorded as shared desktop-runtime work,
not application-specific copy fixes.

## Regression

The Android debug APK builds successfully with JDK 17. All three profiles pass
schema validation, the full desktop rootfs check covers 1,132 ELF objects with
maximum required GLIBC <= 2.41, and the genuine glibc/libX11 core probe remains:

```text
BXSUMMARY passed=32 failed=0 observational_input=yes
```
