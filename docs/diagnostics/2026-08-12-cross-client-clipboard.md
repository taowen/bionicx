# Cross-client X11 clipboard transfer

## Missing and incorrect protocol behavior

The earlier core probe only proved that one connection could set and query a
selection owner. Inspection before testing found three blockers for actual
clipboard data:

- core request 24 `ConvertSelection` was not dispatched;
- `SetSelectionOwner` rejected window `None` as `BadWindow`;
- owner replacement built `SelectionClear` with the new owner window and did
  not notify when one client changed between two of its own windows.

## Controlled real-client workflow

`clipboard-x11-probe` opens two independent AArch64 glibc/libX11 connections
and creates one window on each. It runs the normal ICCCM transfer rather than
copying memory inside the test:

1. connection 1 owns `CLIPBOARD`;
2. connection 2 calls `XConvertSelection` for `UTF8_STRING`;
3. connection 1 receives `SelectionRequest`, writes a property on connection
   2's window, and sends `SelectionNotify` through core `SendEvent`;
4. connection 2 retrieves and byte-compares the 35-byte property;
5. connection 2 replaces the owner and connection 1 validates
   `SelectionClear.window`;
6. ownership is released to `None`, followed by a no-owner conversion whose
   server-generated `SelectionNotify.property` must be `None`.
7. a third connection takes ownership and disconnects; destruction of its
   owner window must atomically clear both the window and owning-client state.

BionicX now routes SelectionRequest to the owning XClient, generates the
no-owner SelectionNotify, accepts `None`, and sends SelectionClear to the
previous owner/window. On x300 all checks passed with zero X errors and the
process exited normally:

```text
BXTEST PASS clipboard-owner owner=0x400001
BXTEST PASS selection-request routed=1
BXTEST PASS clipboard-transfer bytes=35 exact=1
BXTEST PASS selection-clear-none clear=1 none=1
BXTEST PASS owner-disconnect cleanup=1
BXSUMMARY clipboard-x11 passed=5/5 xerrors=0
```

Evidence:

- `evidence/clipboard-x11-probe.log`
- `evidence/clipboard-x11-probe.png`
- `evidence/x11-core-after-clipboard.log` (14/14)
- `evidence/x11-desktop-after-clipboard.log` (8/8)

This covers ordinary bounded UTF-8 selection transfer. `TARGETS`, `MULTIPLE`,
large INCR transfers, timestamp ordering, clipboard persistence after owner
exit, and Android system-clipboard bridging remain separate boundaries.
