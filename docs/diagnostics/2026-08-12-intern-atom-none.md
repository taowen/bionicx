# InternAtom `only_if_exists` must return `None`

## Symptom

After the GTK runtime gained a generated MIME database, `gtk3-probe` decoded
its PNG test icon successfully and advanced into `gtk_window_new()`. GDK then
terminated on this core protocol error:

```text
BadAtom: request_code 16 (InternAtom), minor_code 1
```

The request was a lookup with `only_if_exists=True`. The server represented a
missing name internally as `-1` and incorrectly converted that lookup result
to `BadAtom`.

## Contract and fix

The X11 core protocol defines the InternAtom reply as `ATOM or None`; its only
declared errors are `Alloc` and `Value`. A missing only-if-exists lookup must
therefore return atom zero, while a normal lookup creates the atom.

`AtomRequests.internAtom()` now maps its internal missing sentinel to zero.
The real AArch64 glibc/libX11 probe makes three round trips against a fresh
name: only-if-exists, create, and only-if-exists again. It requires `None`, a
nonzero atom, and the same nonzero atom respectively.

## Device result

On x300 `01408BH601027129`, under the ordinary app UID and without proot:

```text
BXTEST PASS intern-atom-only-if-exists absent=0 created=76 existing=76
BXSUMMARY passed=16 failed=0 observational_input=yes
x11-probe exited with 0
```

The full suite still includes real Android key, tap, and swipe injection, so
this result also guards the existing display/input path against regression.

Protocol reference: <https://www.x.org/archive/current/doc/xproto/x11protocol.pdf>
