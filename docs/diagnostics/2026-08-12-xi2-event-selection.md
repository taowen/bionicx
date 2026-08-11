# XI2 event selection state

After the XKB startup path became clean, WPS repeatedly reached X Input 2
minor opcode 46, `XISelectEvents`. Qt uses this request on multiple windows to
declare interest in master pointer and keyboard events.

BionicX now validates each target window and device selector, parses the
variable-length masks, and stores independent masks per client, window, and
device selector. Empty masks remove a selection. Window/client teardown also
removes the associated state. Minor opcode 60, `XIGetSelectedEvents`, returns
the same interleaved wire representation so state is externally observable.

The genuine AArch64 glibc probe selects key, button, and motion events for
`XIAllMasterDevices`, reads them back through libXi, and checks every bit. The
5/5 zero-X-error result is in `evidence/x11-desktop-probe-xi2-selection.log`.

WPS then starts without an unimplemented XI2 or XKB request in the captured
startup interval; see `evidence/wps-xi2-selection-fixed.log`. Delivery of XI2
GenericEvents from Android input remains a separate capability.
