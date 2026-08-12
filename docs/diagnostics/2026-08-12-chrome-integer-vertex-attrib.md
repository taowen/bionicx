# Chrome integer vertex attributes and reproducible Gladio packaging

Chrome advanced past BGRA shared-image creation and then called
`glVertexAttribIPointer`. The Gladio client and Android renderer had protocol
request identifiers for this entry point, but both handlers were stubs. The
same was true for `glGetVertexAttribiv`, which is needed to verify vertex-array
state.

The client now records client-memory integer arrays and forwards VBO-backed
integer pointers. The renderer records whether an unbound array is integer,
replays it with `glVertexAttribIPointer`, and forwards the VBO and query paths
to host GLES. Calling ordinary `glVertexAttribPointer` clears the integer bit,
so switching an attribute between the two APIs does not retain stale state.

## Gladio source ownership

Gladio is maintained at `github.com/taowen/gladio` on branch `bionicx`. BionicX
now pins commit `087443ef5208c2e0799dfec6ea09b3c59aea0940` as the
`third_party/gladio` Git submodule. `tools/build-gladio.sh` is the single build
entry point used by both the controlled GLX probe and the Chrome bundle. This
replaces the old commit archive download and also removes the former implicit
step where a locally built `libGL.so.1` had to be copied into Chrome by hand.

## Verification

On x300 `01408BH601027129`, the controlled client used a real VBO with two
`GL_INT` components and queried the resulting host state:

```text
BXTEST PASS host-gl-integer-vertex-attrib id=14 integer=1 size=2 type=0x1404 buffer=14 glError=0x0
BXSUMMARY host-glx passed=24 failed=0
BXTEST PASS host-gl-compositor blue=(13, 38, 191) red=(242, 26, 13) outside=(0, 0, 0) size=1920x1080
```

Chrome was then launched with ANGLE OpenGL, Skia Graphite disabled, and the
required `--no-sandbox`. Its log contains neither the former
`unimplemented call glVertexAttribIPointer` nor a `libGL.so.1` load failure.
ANGLE proceeds into repeated complex Skia program links. Those links currently
fail without a driver info log, which is the next bounded OpenGL diagnostic;
it is downstream of integer vertex attribute setup.
