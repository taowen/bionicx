# DRI3 capability honesty and Chrome FD ownership

## Failure

After Chrome/ANGLE passed the GLES 3 capability gate, its GPU process failed
in `base/files/scoped_file.cc` because closing FD 9 returned `EBADF`.  The new
AArch64 diagnostic frame walk normalized pointer-authenticated return addresses
and located the owning path in Chrome file offsets.  Disassembly and the exact
Chrome 151 source identify `ui/gfx/linux/gbm_support_x11.cc` and
`CreateX11GbmDevice()`.

Chrome queried the advertised DRI3 extension and issued `DRI3 Open`.  The X11
server's inherited handler replied with `nfd=0` and sent no `SCM_RIGHTS`
descriptor.  DRI3 Open is specified to return an authenticated DRM descriptor;
Chromium's generated reply parser therefore unconditionally consumes the fd
stored after the X reply.  Advertising the extension without satisfying that
wire contract caused the invalid ownership state.

## Correction

BionicX no longer registers DRI3.  The implementation remains available for a
future Android native-buffer path, but it must not be advertised until Open can
return a real DRM fd and the pixmap/buffer operations have controlled coverage.
Gladio's GLX-to-host-GLES path does not require DRI3.  Present remains exposed
independently.

The core X11 probe now makes this capability boundary executable: it lists the
server extensions and fails if incomplete DRI3 is visible.  On x300
`01408BH601027129` it passes 18/18:

```text
BXTEST PASS list-extensions count=9
BXTEST PASS extension-capability-honesty DRI3 hidden until Open can return a DRM fd
BXSUMMARY passed=18 failed=0 observational_input=yes
```

The following Chrome GPU run logged `dri3 extension not supported`, did not
repeat the FD 9 fatal check, and advanced to real GPU raster/shared-image
creation.  This proves the ownership crash is removed without weakening
Chromium's ScopedFD checks.

## Diagnostic improvement

`bionicx-exec --diagnose-signals` now walks at most 32 monotonic AArch64 x29
frame records.  It strips the x300's pointer-authentication bits to its 39-bit
userspace VA for map lookup, while retaining each raw saved LR.  Every frame is
reported with its ASLR-independent ELF file offset.  Normal launches remain
untraced and pay no cost.
