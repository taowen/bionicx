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

The menu then exposed four related XI2 semantics rather than an application
workaround:

4. `XIGrabDeviceReply` had placed the grab status in byte 1 and left byte 8 as
   padding. The reply now carries minor opcode 51 in byte 1 and status in byte
   8, matching the 32-byte XI2 wire structure.
5. DeviceEvents now include a one-word button-state mask. ButtonPress carries
   the state before the press and ButtonRelease carries Button1 set, and an
   automatic core grab no longer suppresses the grab client's normally
   selected XI2 release.
6. Normal pointer motion previously changed the server's point window without
   emitting core or XI2 crossing events. BionicX now emits Leave/Enter before
   Motion, including the 76-byte XI2 crossing-event form and active-grab
   owner-events routing. GTK immediately selected and activated the menu item.
7. The embedded no-WM focus helper only recognized Winlator-specific
   `WM_HINTS.window_group == id` application windows. It now focuses titled,
   non-override-redirect top-level InputOutput windows, so ordinary GTK
   dialogs receive keyboard input without requiring a per-app focus hack.

Active XI pointer and keyboard grabs retain their XI masks independently and
route owner-events normally or fall back to the grab window. The controlled
desktop probe creates a second top-level window, grabs both master devices,
crosses between windows, and validates the complete wire payload. It reports
two Enter, two Leave, two Motion, two ButtonPress and two ButtonRelease events,
correct before-transition button states, successful pointer/keyboard grabs and
zero X errors. Its Render path also reads back `PictOpSaturate = 0xff`.

## Real application results

- **Ristretto: pass.** The bundle deterministically generates blue and orange
  960x540 PNG fixtures. Ristretto decodes both, creates thumbnails, and a real
  pointer click selects the orange image. The status bar reports
  `bionicx-orange.png`, `960 x 540`, `7.7 kB`, and `51.9%`.
- **Mousepad: pass.** It opens the real bundled note through GtkSourceView,
  receives Ctrl+A, replaces the document with
  `BIONICX_MOUSEPAD_SAVE_PASS`, saves through Ctrl+S, and writes the expected
  26-byte file. After a force-stop and cold restart it reopens and renders the
  saved content. The test fixture was restored afterward.
- **Thunar: pass.** A real pointer opens File and selects Create Folder. The
  GTK dialog maps and receives keyboard input, creates `BX_THUNAR_PASS`, and
  the main view refreshes to show it. Device-side `stat` verifies a real 0700
  directory owned by ordinary app UID `u0_a194`.

Evidence:

- `evidence/bionicx-trixie-ristretto.png`
- `evidence/bionicx-trixie-mousepad.png`
- `evidence/bionicx-trixie-thunar-menu.png`
- `evidence/bionicx-trixie-mousepad-save-reopen.png`
- `evidence/bionicx-trixie-thunar-create-folder.png`

The missing Xfconf/session-bus machine ID and libmagic's absolute
`/usr/share/misc/magic` lookup are recorded as shared desktop-runtime work,
not application-specific copy fixes.

## Regression

The Android debug APK builds successfully with JDK 17. All three profiles pass
schema validation, the full desktop rootfs check covers 1,132 ELF objects with
maximum required GLIBC <= 2.41, and the strict desktop extension probe is:

```text
BXTEST PASS xrender ... saturate=0xff ...
BXTEST PASS xinput2 ... grabs=1/1 ...
BXTEST PASS xi2-device-events motion=2 press=2 release=2 enter=2 leave=2 states=1/1 wire-valid=1
BXSUMMARY desktop-x11 passed=9 failed=0 xerrors=0
```

The genuine glibc/libX11 core probe remains:

```text
BXSUMMARY passed=32 failed=0 observational_input=yes
```
