# Core X11 window-manager save sets

IceWM's last unsupported core request was opcode 6, `ChangeSaveSet`. This is a
lifecycle contract, not optional bookkeeping: a manager inserts foreign client
windows after reparenting them into manager-owned frames. If the manager exits,
the server must rescue those windows before destroying the frames.

BionicX now validates insert/delete mode and ownership, retains each client's
save set, and removes destroyed windows from it. During client teardown it
first reparents every surviving member to the nearest ancestor not owned by the
departing client, preserves root-relative coordinates, emits reparent events,
and maps the rescued window. Only then are the departing client's resources
destroyed.

The genuine AArch64 glibc/libX11 probe creates an app window and manager frame
on independent connections. It exercises insert/delete/reinsert, reparents the
app into the frame, and closes the manager connection:

```text
BXTEST PASS save-set-framed parent=0x800001
BXTEST PASS save-set-manager-disconnect parent=0x4 geometry=220,175 map=2
BXSUMMARY save-set-x11 passed=2/2 xerrors=0
```

The client window survives as a viewable root child at its exact screen
coordinates. A fresh ordinary-app-UID IceWM run remains 3/3 and now produces
zero unsupported core opcodes. Separately, some recognized GrabButton requests
still return `BadImplementation` for cursor overrides; that is tracked as the
next semantic gap rather than hidden by the opcode count.
