# Core X11 server grabs

IceWM uses opcodes 36/37 (`GrabServer`/`UngrabServer`) around multi-request
updates that must be atomic relative to other clients. A no-op implementation
would hide the protocol error without providing that ordering.

BionicX records the grabbing connection before request dispatch. Requests from
other clients remain buffered while the owner continues normally. On explicit
ungrab or owner disconnect, buffered requests are drained even when their
sockets no longer have readable bytes. The same gate covers unauthenticated
new connections, so their X11 setup handshake resumes through the correct
authentication path after release.

The genuine AArch64 glibc/libX11 probe uses two established connections, a
worker thread performing a round-trip `GetInputFocus`, an owner-side
`InternAtom`, a transient grabbing connection, and a connection opened during
the grab:

```text
BXTEST PASS server-grab-peer-frozen exact=1
BXTEST PASS server-grab-owner-progress atom=76
BXTEST PASS server-ungrab-peer-resumed exact=1
BXTEST PASS server-grab-disconnect-release exact=1
BXTEST PASS server-grab-new-connection freeze=1 resume=1
BXSUMMARY server-grab-x11 passed=5/5
```

Core X11 remains 21/21. A fresh ordinary-app-UID IceWM run remains 3/3 and no
longer emits unsupported opcode 36 or 37.
