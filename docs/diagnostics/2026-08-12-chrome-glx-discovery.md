# Chrome GLX discovery and host-GPU capability queries

## Problem progression

Chrome was first run without `--disable-gpu`, while retaining `--no-sandbox`.
The GLX backend initially stopped at three concrete discovery gaps:

- `glXQueryExtension` returned false;
- GLX `GetVisualConfigs` (minor opcode 14) was not implemented;
- the single FBConfig did not identify its X visual.

After those were implemented, ANGLE reached pbuffer initialization. Gladio's
`glXCreatePbuffer` was still a stub, and its GLX extension string did not
describe the ES-profile context that BionicX actually creates on host EGL.

The next diagnostic run found a null-PC `SIGSEGV` in the GPU subprocess. The
link register pointed at Chrome file offset `0x381121c`; disassembly showed a
call through the function pointer loaded for `glGetString(GL_VERSION)`. Gladio
returned ordinary `gl*` entry points only when it was globally loaded. Chrome
loads `libGL.so.1` locally, so the resolver now obtains a handle to its own DSO
before calling `dlsym`.

ANGLE then exposed three real capability-query gaps:
`glGetShaderPrecisionFormat`, `glGetInteger64v`, and `glGetIntegeri_v`. Both
the glibc client and Android renderer sides now forward these queries to the
Qualcomm GLES driver. Gladio also reports its actual backend as
`OpenGL ES 3.2 Gladio`, rather than claiming desktop GL 3.3.

## Diagnostic efficiency

`bionicx-exec --diagnose-signals` now follows fork, vfork, and clone children.
It delivers the two known optional Android seccomp probes to the existing
compatibility handler, then catches the GPU subprocess directly and reports
its registers and ASLR-independent ELF offsets. Normal profiles remain
untraced. A controlled nested-shell crash and the descendant attribution are
recorded in `evidence/diagnose-child-signal.log`.

## Controlled verification

On x300 `01408BH601027129`, the host-GLX probe passes 17/17 checks:

```text
BXTEST PASS glx-gl-proc-address OpenGL ES 3.2 Gladio
BXTEST PASS host-gl-shader-precision range=127..127 precision=23
BXTEST PASS host-gl-integer64 maxElementIndex=2147483647
BXTEST PASS host-gl-indexed-integer maxComputeWorkGroupsX=65535
BXTEST PASS host-gl-identity vendor=Qualcomm renderer=Gladio version=OpenGL ES 3.2 Gladio
BXSUMMARY host-glx passed=17 failed=0
```

Chrome/ANGLE now completes native capability enumeration without crashing or
logging an unimplemented Gladio call. Its remaining host-GPU blocker is that
ANGLE's aggregate maximum supported ES version is still below 3, causing
Chrome to reject the generated EGL config while ES fallback is disabled. The
normal Chrome smoke profile therefore keeps software rendering until that
capability matrix is complete.
