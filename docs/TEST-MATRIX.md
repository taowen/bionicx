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
| ELF/bootstrap and session | `hello-x11`, `loader-argv0-probe`, `session-x11-probe` | glibc loader and explicit `--argv0`, dependency closure, X connection; subreaper adoption, double-fork + `setsid` detached GUI lifetime, normal drain and TERM cleanup | hello passes; loader argv0 plus runtime 21/21; detached session 3/3, adopted child drain and unrooted cleanup pass on x300 |
| X11 core | `x11-probe`, `clipboard-x11-probe`, `keyboard-grab-x11-probe` | windows, properties, drawing, cross-client UTF8 selection transfer and cleanup, async keyboard grab/contention/ungrab/owner-events/disconnect routing, events, modifier ordering | core 14/14, clipboard 5/5, and keyboard grab 7/7 strict checks pass with deterministic Android key/tap/swipe injection |
| X11 desktop | `x11-desktop-probe`, `font-xft-probe` | Render ARGB32 drawing, A8 glyph format, glyph-set upload/lifecycle and CompositeGlyphs8 alpha-over; XFixes selection/regions/ShapeInput, RandR resources/output/CRTC/property/primary/event masks, XInput2 devices/event masks, XKB printable map/event selection/device metadata/names, real xkbcommon compilation and live StateNotify; optional SHM | desktop 8/8 and Fontconfig/Xft 4/4 pass; MIT-SHM remains pending |
| libc/kernel | `runtime-probe` | threads, robust owner death, epoll, signals, processes, IPC, sockets, mmap | 20/20 passing on x300 with BionicX glibc 2.39 |
| glibc/Bionic bridge | `wps-compat-probe`, `network-x11-probe` | WPS SysV semaphore shim and sanitized Android-shell `popen`; Android active-network DNS injection into glibc `res_state`, live DNS/TCP/HTTP and X11 result rendering | WPS 21/21 and network 5/5 passing untraced under the ordinary app UID |
| desktop services | `font-xft-probe`, `bionicx-open`, WPS workflow plus planned probes | app-private Fontconfig discovery, separate regular/bold FreeType faces and real Xft rendering; deterministic Calibri/Cambria/Arial/Times aliases to static Liberation fonts; glibc URI dispatcher and PDF handler; locale, broader MIME classes, D-Bus alternatives, audio | controlled Fontconfig/Xft 4/4 and WPS Latin regular/bold cold reopen pass; Writer export opens in detached WPS PDF through the unrooted app-private dispatcher; remaining services pending |
| Chromium | Google Chrome stable 151.0.7922.108 ARM64 | direct patched-PT_INTERP bootstrap, recursive dependency closure plus identified runtime-loaded NSS modules, ICU `/proc/self/exe` path, XKB partial-map parsing, Android seccomp feature probes, child Crashpad-policy propagation, app-private Fontconfig plus Android/Liberation fonts, Android DNS bridge, X11 UI; sandbox, services and GPU/software rendering | genuine, fully legible browser loads Example Domain through DNS/TCP/TLS and renders it untraced under the ordinary app UID; fullscreen, runtime-module closure automation and remaining services are pending |
| office | real WPS ARM64 | Writer, Sheets, Presentation, PDF import/export, clipboard | Writer durable copy/paste/cold reopen, structurally verified PDF export, and post-export Open File all pass; Spreadsheets creates/calculates/saves/cold-reopens valid OOXML; Presentation edits, saves, structurally verifies, presents full-screen, exits via Escape, and cold-reopens valid PPTX untraced; WPS PDF opens a structurally checked two-page fixture, renders exact content, navigates, zooms, and cold-reopens untraced |

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
