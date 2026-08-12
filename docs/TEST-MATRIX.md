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
| X11 core | `x11-probe`, `x11-window-tree`, `clipboard-x11-probe`, `keyboard-grab-x11-probe`, `pointer-grab-x11-probe`, `server-grab-x11-probe`, `save-set-x11-probe`, `icewm-probe` | windows including resolved `CopyFromParent` class and legal `InputOnly` geometry, direct-child `MapSubwindows`, delayed viewability exposure and initial visibility notification, `InternAtom(only_if_exists)` returning `None`, honest extension advertisement (incomplete DRI3 hidden), cross-process recursive window observation, property round trips and enumeration, cursor-font resource lifecycle, drawing including GC-clipped `CopyArea`, PolyPoint origin/previous coordinates and tiled `CWBackPixmap` clear semantics, cross-client UTF8 selection transfer and cleanup, active/passive keyboard and pointer grabs with contention/ungrab/owner-events/disconnect/automatic-release routing, cursor override, synchronous freeze and `ReplayPointer` click-through, exact pointer event state, server-grab request/setup deferral and disconnect cleanup, WM save-set rescue/reparent/map/coordinate preservation, events, modifier ordering; real WM root `SubstructureRedirect`, request-owner exclusion, reparent coordinates/events and nested mapping; declared runtime-loaded Imlib2 plugin closure | core 24/24, Chrome window tree with zero X errors, clipboard 5/5, keyboard grab 9/9, pointer grab 6/6, server grab 5/5, save set 2/2, and unmodified Debian ARM64 IceWM starting a fully painted taskbar, managing two independent glibc clients, and painting two title bars plus four control groups 4/4 all pass with zero unsupported opcodes and zero request errors; cursor-font glyph shapes are approximate, dynamic partial/full visibility changes and other `AllowEvents` modes are pending |
| X11 desktop | `x11-desktop-probe`, `font-xft-probe`, `gtk3-probe` | Render ARGB32/A8 pictures, creation-time repeat, rectangle and 1-bit pixmap clips with clip origins, filters, solid/gradient sources, Clear/Src/Over/In/OutReverse/Add pixel semantics, glyph-set upload/lifecycle and CompositeGlyphs8 alpha-over; XFixes mask-7 selection owner/set/window-destroy/client-close lifecycle, regions and ShapeInput; RandR resources/output/CRTC/property/primary/event masks; XInput2 master ButtonClass, XIQueryPointer, event masks and live DeviceEvents; XKB printable map/event selection/device metadata/names, real xkbcommon compilation and live StateNotify; optional SHM | desktop 9/9 with zero X errors and exact pixmap-clip inside/outside pixels, Fontconfig/Xft 4/4, real GTK3/Pango/label lifecycle 10/10, and five deterministic complete file-chooser paints pass; MIT-SHM remains pending |
| Host GPU | `glx-probe`, `vulkan-probe` | Real glibc clients through pinned BionicX Gladio and Vortek submodules; complete GLX/GLES capability, shader, cache, draw, readback, and compositor coverage; optional app-private Vulkan host service, glibc Vulkan loader/ICD discovery, shared-ring RPC into a Bionic process, native vendor physical-device/queue/memory discovery, honest device-extension filtering, Xlib/XCB WSI translation, logical-device/swapchain commands, coherent mapped vertex/index/uniform/staging upload, buffer-to-image copies and barriers, sampled-image descriptors, SPIR-V graphics pipelines/render passes/indexed draws, Vulkan 1.3 vertex binding with exact optional-array NULL fidelity, present-semaphore ownership, and imported `AHardwareBuffer` presentation | GLX client 26/26 plus final Android screenshot pixel assertion pass; Chrome's normal untraced profile displays its complete UI through ANGLE/Ganesh OpenGL; Vulkan client 35/35 binds both Xlib and XCB surfaces to a real 640x360 X window, verifies unimplemented fragment shading rate is hidden, uploads mapped buffers, stages and samples a texture, binds uniform and combined-image descriptors, executes an indexed `vkCmdBindVertexBuffers2` draw with NULL sizes/strides, consumes the present semaphore without a pre-present idle, and passes exact two-color Android screenshot assertions through `/vendor/lib64/hw/vulkan.adreno.so`; Chrome ANGLE Vulkan creates 68 observed graphics pipelines successfully, while multi-image swapchain lifecycle remains under investigation |
| libc/kernel | `runtime-probe` | threads, robust owner death, epoll, signals, processes, IPC, sockets, mmap | 20/20 passing on x300 with BionicX glibc 2.39 |
| glibc/Bionic bridge | `wps-compat-probe`, `network-x11-probe` | WPS SysV semaphore shim and sanitized Android-shell `popen`; Android active-network DNS injection into glibc `res_state`, live DNS/TCP/HTTP and X11 result rendering | WPS 21/21 and network 5/5 passing untraced under the ordinary app UID |
| desktop services | `font-xft-probe`, `bionicx-open`, WPS workflow plus planned probes | app-private Fontconfig discovery, separate regular/bold FreeType faces and real Xft rendering; deterministic Calibri/Cambria/Arial/Times aliases to static Liberation fonts; compiled app-private GTK GSettings schemas with memory backend; glibc URI dispatcher and PDF handler; locale, broader MIME classes, D-Bus alternatives, audio | controlled Fontconfig/Xft 4/4 and WPS Latin regular/bold cold reopen pass; Chrome has zero schema/dconf errors; Writer export opens in detached WPS PDF through the unrooted app-private dispatcher; remaining services pending |
| Chromium | Google Chrome stable 151.0.7922.108 ARM64 | pinned/hash-locked package acquisition, direct patched-PT_INTERP bootstrap, recursive dependency closure plus declared runtime-loaded NSS roots, ICU `/proc/self/exe` path, XKB partial-map parsing, Android seccomp feature probes, child Crashpad-policy propagation, app-private Fontconfig plus Android/Liberation fonts, Android DNS bridge, deterministic 1920x1080 X11 window, ANGLE/Ganesh OpenGL on host GLES; separate ANGLE Vulkan profile through Vortek; sandbox and services | the normal profile loads Example Domain through DNS/TCP/TLS and renders a complete hardware-accelerated full-screen UI untraced under the ordinary app UID; ANGLE Vulkan creates its device, acquires the Queue2 graphics queue, creates an AHardwareBuffer swapchain, remains alive, renders the complete browser and internal GPU page without black regions, accepts Android-injected navigation, and reports hardware-accelerated Canvas/compositing/raster/WebGL/WebGPU on the Vortek Adreno Vulkan device; `--no-sandbox` is intentionally retained and remaining services are pending |
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
