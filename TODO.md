# BionicX TODO

This is the ordered implementation backlog for the fixed Debian 13 trixie
ARM64 2026-08-11 snapshot. Every accepted result must run as the ordinary
Android application UID, without PRoot, Termux, runtime root, Frida, an
application-specific library copy, or a compatibility fallback.

For each capability: reproduce the gap with a controlled glibc client, retain
diagnostic evidence, add a regression test, verify real applications untraced,
then commit and push the smallest complete change.

## P0: package transactions

- [x] Reproduce the shadow-tools `/etc/group.lock` failure with a controlled
  account-creation probe and fix the shared FHS/locking contract. A clean
  `bxapt install cups-daemon cups-client` must configure package accounts
  without pre-creating them with `systemd-sysusers` as a recovery step.
- [x] Add an explicit interrupted-transaction recovery path to `bxapt`. It must
  finish unpacked/half-configured packages, rerun incremental ELF normalization
  where required, and reconcile apt manual/automatic marks from the retained
  pre-transaction snapshot.
- [x] Exercise install, failed configure, recovery, remove and autoremove from
  a clean seed using a package that creates system users/groups. Require an
  empty `dpkg --audit`, correct account files, correct apt marks and a pruned
  ELF ledger after every final state.
- [ ] Rebuild and publish the reproducible seed with the fixed Android glibc
  identity namespace, then reconstruct the shared device rootfs only through
  the pinned seed plus `bxapt` declarations. The identity-fixed seed is on
  `01408BH601027129` (`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`);
  the popular/WPS/Chrome cohort has not been reinstalled through `bxapt set`.

## P1: shared desktop services

- [x] Add app-private CUPS supervision as a shared desktop service. cupsd must
  use private configuration, run, spool and socket paths and survive the same
  force-stop/restart lifecycle tests as the session D-Bus service.
- [x] Create one controlled local printer destination without requiring real
  hardware. Verify `cupsGetDests()` and a submitted test job before using WPS
  as the diagnostic surface.
- [ ] Verify WPS discovers the destination, removes its misleading CUPS
  warning, opens the print dialog and submits a document successfully.
- [ ] Turn IceWM plus the existing D-Bus, audio, opener, CUPS and GPU services
  into one reusable desktop session. Verify two unrelated package-installed
  applications can be launched, switched, resized, closed and reopened without
  changing profiles or duplicating dependencies.

## P1: GPU completion

- [ ] Complete Vortek multi-image swapchain acquire/present/recreate lifetime
  handling and add repeated resize/background/foreground regression coverage.
- [ ] Revalidate Chrome ANGLE Vulkan without tracing: normal browsing, WebGL,
  video/compositing, resize and cold restart. Keep `--no-sandbox`.
- [ ] Keep Gladio OpenGL and Vortek Vulkan as pinned submodules and verify both
  against the device's host Mali/Adreno driver path without VirGL fallback.

## P2: WPS completeness

- [ ] Build a controlled formula-glyph and Fontconfig-family probe for the
  identities WPS checks. Implement deterministic aliases only after verifying
  glyph coverage; do not bundle proprietary fonts or merely suppress the
  warning.
- [ ] Rerun Writer, Sheets, Presentation and PDF workflows from a clean shared
  rootfs, including open/edit/save/cold-reopen, clipboard, export, print and
  full-screen presentation.

## P2: Debian application coverage

- [ ] Complete online navigation for Firefox ESR and repeat cold-start/network
  acceptance on a device with working connectivity.
- [ ] Complete real workflows for Krita, qBittorrent and KeePassXC, including
  durable file/database state and the relevant network or graphics path.
- [ ] Rebuild and rerun the accepted xterm, IceWM, Thunar, Mousepad, Ristretto,
  LibreOffice Writer, Evince, GIMP, Inkscape, VLC, Geany, FileZilla and
  Thunderbird profiles from the same clean seed and package declarations.
- [ ] Add further popular ARM64 trixie applications only when they extend
  coverage of a shared capability rather than introducing a per-app runtime.

## Release checkpoint

- [ ] Verify install, upgrade/reinstall and removal leave one consistent dpkg
  database and no duplicated system libraries across every declared app.
- [ ] Run the complete controlled test matrix and real application workflows
  after an Android force-stop and device reboot, with diagnostics disabled.
- [ ] Document remaining Android-kernel limitations honestly and provide a
  reproducible build/install/desktop-use guide for a new device.
- [ ] Declare the goal complete only when Debian popular applications, WPS and
  Google Chrome meet their full functional acceptance criteria on the shared
  rootfs.
