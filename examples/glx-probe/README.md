# Host GPU GLX probe

This genuine AArch64 glibc/X11 client links the
[BionicX Gladio fork](https://github.com/taowen/gladio/tree/bionicx), pinned as
the `third_party/gladio` Git submodule. It reports five stages: GLX setup,
visual/FBConfig/pbuffer, context, GLES capabilities used by Chrome/ANGLE,
and present/readback. It creates
a real GLX context, asks the host GLES driver for its
identity, draws a fixed-function triangle, reads center/background pixels back
from the GPU, and swaps the drawable into the embedded X server window.
Before context creation it verifies GLX discovery and version negotiation, then
sends a raw GLX 1.2 `GetVisualConfigs` request and validates the positional
18-property wire reply used by Chromium's ANGLE GLX backend.
It also covers the three tagged FBConfigs (double-buffer plus two
SingleBuffer Qt configs): every advertised config must carry the X visual
id and `GLX_X_RENDERABLE`. It covers pbuffers, ordinary `gl*` entry points
returned from `glXGetProcAddress`, shader precision, and indexed/64-bit
capability queries used by real GL loaders. The string and numeric OpenGL ES
versions are checked independently so loader-visible metadata cannot drift.
The two values returned for `GL_MAX_VIEWPORT_DIMS` are checked independently;
losing the height makes ANGLE silently clamp every Chrome viewport to zero.
Its modern GLSL test preserves integer assignment targets and the standard
`gl_VertexID` built-in while translating an OpenGL ES 3 shader pair.
Required GLES 3 renderbuffer formats must also expose the host driver's real
multisample counts; ANGLE rejects GLES 3 if any non-integer required format
cannot provide at least 4x MSAA.
The probe also resolves and exercises the complete transform-feedback object
lifecycle required when a loader accepts the advertised GLES 3 version.
It requires the Chromium-recognized BGRA texture extension name and verifies
that a `GL_BGRA` texture is a complete framebuffer color attachment with
correct host-GPU clear/readback channel ordering. This is the backing format
used by Chromium's Linux GPU raster tiles.
Finally, a real linked GLSL program is exported with `glGetProgramBinary`,
restored into a second program with `glProgramBinary`, and checked for a
successful link. Chromium uses this path for its ANGLE program cache.

Gladio exercises Winlator's host-GLES command path. It is distinct from the
zero-copy DRI3/Present `AHardwareBuffer` path, which needs a separate controlled
Mesa client.

```sh
ANDROID_SERIAL=<serial> examples/glx-probe/install-and-run.sh
```

The runner checks both GPU readback inside the glibc client and pixels from an
Android screenshot after the shared texture has passed through the embedded
X11 compositor. Set `BIONICX_SCREENSHOT=path.png` to retain that screenshot.
