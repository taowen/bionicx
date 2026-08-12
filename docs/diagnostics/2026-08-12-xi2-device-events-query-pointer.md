# XI2 DeviceEvents and QueryPointer

Chrome's GTK Save Page dialog painted correctly but did not react to clicks.
The GTK windows selected XI2 events without selecting the corresponding core
pointer events, while BionicX only stored XISelectEvents masks and never
delivered GenericEvents.

BionicX now routes master pointer and keyboard DeviceEvents according to each
client's XIAllDevices, XIAllMasterDevices, or exact-device selection. The event
uses the protocol's fixed 80-byte DeviceEvent representation and reports the
same master device as both `deviceid` and `sourceid`. XIQueryDevice also exposes
a seven-button ButtonClass for the master pointer.

After event delivery was added, clicking GTK's Save button made Chrome exit
with SIGTRAP. A root-only diagnostic strace showed the actual trigger:

```
request: XInput opcode 149, minor 40 (XIQueryPointer)
reply:   X error 17 (BadImplementation)
result:  Chrome BRK/SIGTRAP
```

Implementing XIQueryPointer with root/child windows, 16.16 coordinates,
modifier state and an empty button-mask reply removes that protocol error. The
controlled real AArch64 glibc/libXi client now proves ButtonClass,
XIQueryPointer, selection round-trip, and live XI_Motion/ButtonPress/
ButtonRelease delivery. `evidence/x11-desktop-probe-xi2-events.log` records the
9/9 zero-error result.

The same untraced Chrome 151 workflow then loaded Example Domain, opened the
native GTK Save Page dialog, accepted the Save click, remained alive, and wrote
`Downloads/example.com.html` (155941 bytes). Evidence is retained in
`evidence/chrome-gtk-save-dialog-xi2.png`,
`evidence/chrome-gtk-save-complete.png`, and
`evidence/chrome-gtk-save-complete.log`. Chrome continues to launch with
`--no-sandbox` as required.
