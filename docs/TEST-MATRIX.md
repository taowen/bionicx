# Integration test matrix

## Workflow

Each gap follows the same loop:

1. reproduce it with the smallest genuine glibc client possible;
2. retain raw symptoms and the decisive trace in `docs/diagnostics/`;
3. add a machine-readable `BXTEST` regression;
4. implement the smallest runtime or X server correction;
5. verify on Android, commit that single capability, and push it;
6. rerun all lower layers before retrying Chrome or WPS.

`PASS` means a request completed with the expected observable result, not just
that the client did not crash. Interactive checks record screenshots and input
counters. App profiles always run with diagnostics disabled for final proof.

## Layers

| Layer | Controlled client | Coverage | State |
|---|---|---|---|
| ELF/bootstrap | `hello-x11` | glibc loader, dependency closure, X connection | passing on x300 |
| X11 core | `x11-probe` | windows, properties, drawing, selection, events, modifier event-state ordering | 14/14 strict checks pass; deterministic Android key/tap/swipe injection |
| X11 desktop | `x11-desktop-probe` | Render, XFixes, RandR resources/primary, XInput2 devices/event masks, XKB printable map/event selection/device metadata/names, real xkbcommon compilation and live StateNotify; optional SHM | 7/7 passing; MIT-SHM deliberately unavailable pending safe backend |
| libc/kernel | `runtime-probe` | threads, robust owner death, epoll, signals, processes, IPC, sockets, mmap | 20/20 passing on x300 with BionicX glibc 2.39 |
| glibc/Bionic bridge | `wps-compat-probe` | WPS SysV semaphore shim and sanitized Android-shell `popen` | 21/21 passing; Bionic shell output/exit status verified |
| desktop services | planned probes | fonts, locale, MIME, D-Bus alternatives, audio | pending |
| Chromium | real Chrome ARM64 | sandbox, zygote, SHM, network, TLS, GPU/software rendering | pending |
| office | real WPS ARM64 | Writer, Sheets, Presentation, PDF import/export, clipboard | Writer opens/creates a document, Qt compiles XKB keymap/state, and mixed-case/shifted Android input is exact; save/reopen and other workflows remain |

## Upstream acceptance baseline

Baseline inspected on 2026-08-11 at Pi-Apps commit
`e84fe19d038c310502cbd2e296c549d95729ee4f`.

- Chrome `install-64` installs Google's ARM64 stable/beta/unstable/canary Debian
  packages from `https://dl.google.com/linux/chrome-stable/deb/`. BionicX will
  begin with stable and record its exact package version and hash.
- WPS `install-64` selects `wps-office_11.1.0.11720_arm64.deb`, adds legacy
  WebP/TIFF dependencies for PDF export, Microsoft-compatible fonts, X11
  utilities and `wmctrl`. Pi-Apps removes `wpscloudsvr` and runs WPS offline;
  BionicX preserves that privacy-oriented acceptance baseline.

The upstream scripts are acquisition specifications, not scripts to execute
inside Android. Application payloads and proprietary packages are never
committed to this repository.
