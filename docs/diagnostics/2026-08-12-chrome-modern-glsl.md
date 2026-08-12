# Preserve modern integer GLSL in the Gladio converter

After integer vertex attributes were forwarded, Chrome's GPU process reached
Skia program creation but every complex program failed to link. ANGLE could
only report that the driver returned no program info log because Gladio
deferred the real GLES shader compile until its link handler and discarded the
underlying shader failure.

The renderer now logs the real GLES compile/link error and the exact converted
source line on failure. This immediately exposed two converter defects rather
than a host GPU limitation:

```text
ERROR: 0:59: 'assign' : l-value required
float(_64_pos) = ...;

ERROR: 0:14: 'gd_VertexID' : undeclared identifier
vec2(float(gd_VertexID & 1), ...)
```

The first came from applying a desktop GLSL 1.20 implicit int-to-float repair
to a `#version 300 es` shader. It converted the left side of an integer
assignment into a constructor expression. The second came from prefix matching
`gl_VertexID` as the legacy fixed-function name `gl_Vertex`. Reserved built-ins
are now matched exactly, except for the intentionally indexed
`gl_MultiTexCoordN` family, and implicit numeric repair is restricted to GLSL
versions before 1.30. Standard modern built-ins pass through unchanged.

## Controlled verification

The host-GLX probe now compiles and links an ES 3 vertex/fragment pair that
uses both an integer assignment target and `gl_VertexID`. On x300
`01408BH601027129`:

```text
BXTEST PASS host-gl-modern-integer-shader vertex=1 fragment=1 linked=1
BXSUMMARY host-glx passed=25 failed=0
BXTEST PASS host-gl-compositor blue=(13, 38, 191) red=(242, 26, 13) outside=(0, 0, 0) size=1920x1080
```

Chrome was then run with `--no-sandbox`, ANGLE OpenGL, and Ganesh. After eight
seconds its GPU and renderer processes remained alive, with zero Gladio shader
compile failures, zero Skia shader compilation errors, and zero integer vertex
attribute stubs. The Android screenshot is still black except for the X cursor.
This moves the next investigation from shader compilation to final Chrome
surface presentation without conflating the two stages.
