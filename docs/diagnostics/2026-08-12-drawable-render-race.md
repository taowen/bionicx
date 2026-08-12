# Drawable-to-GL presentation race

## Symptom and rejected GPU hypothesis

After the protocol fixes, `GetImage` samples from the GTK window backing store
were correct while Android screenshots intermittently showed an empty or
partially stale window. The ordinary X11 window used `Texture`, not
`GPUImage`, so this was the CPU-upload presentation boundary.

Winlator's two paths were compared directly:

- ordinary X11 drawing uploads a Java direct `ByteBuffer` with
  `glTexSubImage2D`;
- DRI3/Present replaces the drawable texture with `GPUImage`, whose
  `AHardwareBuffer` is imported as an `EGLImageKHR` and sampled by the host GPU.

Instrumentation read `GL_PIXEL_UNPACK_BUFFER_BINDING`, row length, and pixel/row
skip before every GTK upload. All remained at their defaults. Resetting them
did not make the result deterministic, so no host-driver workaround was kept.

## Root cause and correction

The GL thread uploads a drawable while holding `Drawable.renderLock`, but
several X11/XRender writers released that lock before setting `needsUpdate`.
This ordering permits the renderer to clear the flag after the writer has set
it, losing the final damage frame even though the CPU pixels are correct.

Pixmap copy, fills, and Render blend operations now mutate pixels and mark the
texture dirty in the same critical section used by texture upload.

Five consecutive cold GTK starts were synchronized after creation and allowed
two seconds to paint. Every screenshot returned white at all three content
samples `(500,400)`, `(200,150)`, and `(1000,150)`:

```text
run 1..5: srgba(255,255,255,1) at all three samples
BXTEST PASS pango-font Roboto 14
BXTEST PASS gtk-label BionicX GTK3 text rendering
BXSUMMARY gtk3 passed=10/10 failed=0
```

The host-GPU path remains the right target for real OpenGL clients, but it is a
separate DRI3/Present integration task and was not used to hide this CPU-path
race.
