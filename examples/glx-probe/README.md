# Host GPU GLX probe

This genuine AArch64 glibc/X11 client links the pinned
[BionicX Gladio fork](https://github.com/taowen/gladio/tree/bionicx). It creates
a real GLX context, asks the host GLES driver for its
identity, draws a fixed-function triangle, reads center/background pixels back
from the GPU, and swaps the drawable into the embedded X server window.
Before context creation it verifies GLX discovery and version negotiation, then
sends a raw GLX 1.2 `GetVisualConfigs` request and validates the positional
18-property wire reply used by Chromium's ANGLE GLX backend.
It also covers FBConfig visual metadata, pbuffers, ordinary `gl*` entry points
returned from `glXGetProcAddress`, shader precision, and indexed/64-bit
capability queries used by real GL loaders. The string and numeric OpenGL ES
versions are checked independently so loader-visible metadata cannot drift.
Required GLES 3 renderbuffer formats must also expose the host driver's real
multisample counts; ANGLE rejects GLES 3 if any non-integer required format
cannot provide at least 4x MSAA.
The probe also resolves and exercises the complete transform-feedback object
lifecycle required when a loader accepts the advertised GLES 3 version.

Gladio exercises Winlator's host-GLES command path. It is distinct from the
zero-copy DRI3/Present `AHardwareBuffer` path, which needs a separate controlled
Mesa client.

```sh
ANDROID_SERIAL=<serial> examples/glx-probe/install-and-run.sh
```

The runner checks both GPU readback inside the glibc client and pixels from an
Android screenshot after the shared texture has passed through the embedded
X11 compositor. Set `BIONICX_SCREENSHOT=path.png` to retain that screenshot.
