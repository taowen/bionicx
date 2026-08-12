# Core X11 asynchronous keyboard grabs

## Symptom isolated from WPS

Opening Writer's Paste drop-down emitted core opcode 31 and the server returned
`BadImplementation`:

```text
WinlatorXRequest: seq=5765 opcode=31 data=0 bytes=12
WinlatorXRequest: unsupported opcode=31 data=0
java.lang.UnsupportedOperationException: Unsupported opcode 31.
```

Opcode 31 is `GrabKeyboard`; opcode 32 is `UngrabKeyboard`. The menu remained
mouse-operable, but a general X11 popup relies on this boundary to receive keys
while another window has focus. WPS alone could not prove the routing semantics.

## Controlled implementation and result

`keyboard-grab-x11-probe` is a genuine AArch64 glibc/libX11 application with
independent grabber and peer connections. Its installer waits for explicit
device-log barriers and injects A, B, and C through Android input:

- A is rerouted to the grab window while the peer owns input focus;
- B reaches the focused peer after `UngrabKeyboard`;
- C reaches another normally selected window on the grabbing connection when
  `owner_events=True`.

It additionally requires `GrabSuccess`, `AlreadyGrabbed`, `GrabNotViewable`,
and release after the grabbing connection disconnects. On x300 serial
`01408BH601027129` the real client reported:

```text
BXTEST PASS keyboard-grab status=0
BXTEST PASS keyboard-contention status=1
BXTEST PASS keyboard-not-viewable status=3
BXTEST PASS keyboard-grab-route exact=1
BXTEST PASS keyboard-ungrab-route exact=1
BXTEST PASS keyboard-owner-events normal-route=1
BXTEST PASS keyboard-owner-disconnect cleanup=1
BXSUMMARY keyboard-grab-x11 passed=7/7 xerrors=0
keyboard-grab-x11-probe exited with 0
```

The server keeps keyboard-grab ownership separately from pointer grabs, routes
key press and release events to the correct connection/window, and clears the
state on unmap or client teardown. This checkpoint qualifies asynchronous modes
with `CurrentTime`. Synchronous modes need opcode 35 `AllowEvents` freeze/thaw,
and non-current timestamp ordering needs a server-time model; those variants
are explicitly rejected rather than silently claimed.

## Regression and real-application check

After installing the change, the existing core probe passed 14/14 and the
cross-client clipboard probe passed 5/5, both with zero X errors and exit 0.
The genuine WPS Paste menu then emitted opcode 31 with no unsupported request,
`BadImplementation`, crash, or signal. An Android-injected Escape reached the
grabbed popup and closed it; unmapping that grab window cleared server state.
`UngrabKeyboard` itself is exercised directly by the controlled probe:

```text
WinlatorXRequest: seq=2488 opcode=31 data=0 bytes=12
BXASSERT Escape closed the grabbed WPS Paste popup
```

Evidence is retained in `evidence/keyboard-grab-x11-probe.log`,
`evidence/keyboard-grab-x11-probe.png`,
`evidence/wps-keyboard-grab-menu.log`, and
`evidence/wps-keyboard-grab-menu.png`, with the Escape result in
`evidence/wps-keyboard-grab-escape.png`.

## Passive desktop shortcuts

IceWM subsequently exposed core opcodes 33/34 (`GrabKey`/`UngrabKey`) 198 times
during startup. The server now retains passive grabs per client and window,
matches exact keys plus `AnyKey` and exact modifiers plus `AnyModifier`, walks
from the focused window through its ancestors, and converts a match into an
active asynchronous grab. It releases only after the logical keyboard becomes
empty and removes registrations on client disconnect or window destruction.
Overlapping registrations owned by another client return `BadAccess`.

The controlled probe installs an `AnyModifier` D grab on the root from one
connection while a peer window has focus. D is delivered only to the grabber;
after its release, E reaches the peer through normal focus routing:

```text
BXTEST PASS passive-key-grab route=1
BXTEST PASS passive-key-grab auto-release=1
BXSUMMARY keyboard-grab-x11 passed=9/9 xerrors=0
```

A fresh ordinary-app-UID IceWM run still passes 3/3 and emits neither opcode 33
nor opcode 34 as unsupported. Passive synchronous modes remain outside this
checkpoint because they require `AllowEvents` freeze/thaw semantics.
