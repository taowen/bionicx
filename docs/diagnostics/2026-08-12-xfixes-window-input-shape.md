# XFixes SetWindowShapeRegion input semantics

## Trigger

Cold-reopening the saved DOCX advanced WPS to XFixes minor opcode 21,
`SetWindowShapeRegion`. BionicX returned `BadImplementation`. Qt uses the
ShapeInput kind here, so merely acknowledging the void request would hide an
observable pointer-routing requirement.

## Controlled test

The genuine AArch64 glibc desktop probe now creates a 720x420 window and an
XFixes region covering only its left 360 pixels. It applies that region as
ShapeInput, destroys the region resource before input arrives, and selects
ButtonPress. The host injects one tap inside the rectangular window but outside
the input shape, then one inside the shape. Exactly one event must arrive at
the client. This simultaneously proves hit clipping and the protocol-required
copy lifetime.

`evidence/x11-desktop-probe-xfixes-input-shape.log` records 8/8 strict passes.

## Implementation boundary

Windows hold an immutable copied input region with request offsets. Pointer
hit testing intersects normal window bounds with that region; an empty region
accepts no input, and region ID `None` restores the default rectangular input
shape. The published shape is volatile so request and Android input threads see
a coherent immutable snapshot.

The server deliberately continues to reject ShapeBounding and ShapeClip for
this request: those kinds require compositor clipping that has not yet been
implemented. They are not needed by this WPS path and are not falsely
advertised.

## WPS result

The same cold-start/OpenFile path no longer emits XFixes minor 21 and remains
interactive. `evidence/wps-xfixes-input-shape.log` retains the remaining minor
2 requests, making the absence of 21 auditable without claiming the other
selection-notification operation is implemented.
