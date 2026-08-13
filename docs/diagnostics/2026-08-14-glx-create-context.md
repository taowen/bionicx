# Gladio CreateContext after cold start

`glXCreateContext` / `glXCreateNewContext` returned NULL after
`adb reboot` (`glx-probe` 9/1 `create-null`, `krita-glx-destroy` 2/2
`create-new`). FBConfig visuals were already fine (`configs=3
visual=0x1 xRenderable=1`).

`createGLXContext` shared the compositor `GLSurfaceView` EGL context
and waited only 5s. After reboot the activity often starts behind the
keyguard (`isSleeping=true`), so `onSurfaceCreated` never runs and the
wait times out.

## Fix

If the compositor context is missing, create an unsared GLES3 context
instead of returning NULL. `requestRender()` on resume still tries to
publish the compositor context when the display is on.

## Device

Unlocked, `glx-probe` is 26/26 including compositor blue/red pixels
(`renderer EGL context ready`). `krita-glx-destroy-probe` is 4/4.
After reboot with the screen later unlocked, logcat still shows
`BXSUMMARY host-glx passed=26 failed=0`. A sleeping/keyguard launch
logs `timed out waiting for compositor EGL context` and still creates
the unsared context (`glx-context current`).

Evidence: `evidence/rebuild-2026-08-14/glx-probe-after-fix.log`,
`krita-glx-destroy-after-fix.log`, `glx-probe-reboot.log`.
