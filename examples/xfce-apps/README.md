# XFCE desktop application cohort

This integration cohort runs three real Debian trixie ARM64 applications from
the same pinned apt/dpkg rootfs as Chrome, WPS, IceWM and xterm:

- Thunar exercises file management, MIME/GIO integration and multi-window UI.
- Mousepad exercises GTK text editing, save/reopen and settings persistence.
- Ristretto exercises image decoding, scaling and keyboard navigation.

They deliberately share one package transaction and one immutable runtime;
there are no application-specific copied dependency trees.

Build once and select an application by installing its profile:

```sh
examples/xfce-apps/build-bundle.sh
tools/install-profile.sh --profile profiles/thunar.json \
  --app-root build/xfce-apps-bundle/app \
  --runtime-root build/xfce-apps-bundle/rootfs
```

Replace `thunar.json` with `mousepad.json` or `ristretto.json` for the other
applications. Acceptance requires an untraced launch under the ordinary
Android app UID plus an application-specific workflow: a real file operation,
save/reopen, or opening and navigating images respectively.

All three workflows pass on x300: Thunar creates and displays a real directory,
Mousepad edits, saves and cold-reopens a document, and Ristretto decodes and
navigates two PNG images. See
`docs/diagnostics/2026-08-13-trixie-xfce-apps.md`.
