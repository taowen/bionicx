# Chrome ANGLE Vulkan and Gladio on Adreno

Device `01408BH601027129` kept seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.
`bxapt deb` installed hash-pinned Chrome `151.0.7922.108-1`
(`23f5d27be6ad6f5d69c1c11b602d4ed25a8499cfdfa11c3ca479ad0b58285499`);
`dpkg --audit` stayed empty.

Untraced `chrome-vulkan.json` (`--no-sandbox --use-angle=vulkan`) creates an
ANGLE instance on `/vendor/lib64/hw/vulkan.adreno.so`. Multiprocess
renderer/network children abort with Chrome's FD-ownership checker, so the
Vulkan test profile also passes `--single-process`. The controlled
`webgl-fixture.html` then reports `WEBGL_OK` and paints a green canvas.
`https://example.com/` loads Example Domain. A display-size change and a
force-stop cold start keep the same ANGLE/Adreno path. No VirGL strings
appear.

`glx-probe` now installs with `--app-root` only. It is 26/26 plus compositor
pixels through Gladio on `/vendor/lib64/egl/libGLESv2_adreno.so`.

Post-reconstruct recapture (2026-08-14) on the same seed: `vulkan-probe`
40/40 including swapchain acquire-rotate / resize-outdated / recreate /
foreground, compositor triangle on Vortek (Adreno 750). `glx-probe`
compositor blue/red still pass; the summary is 25/26 because
`glx-fbconfig-visual` reports `configs=3 visual=0x0 xRenderable=0` on both
cold and warm start. Untraced Chrome Vulkan still shows `WEBGL_OK`, Example
Domain, a 1600x900 resize, and a force-stop cold start on
`/vendor/lib64/hw/vulkan.adreno.so` with no VirGL.

The `visual=0x0` line was a probe short-circuit: `fbconfig_count == 1`
skipped `glXGetFBConfigAttrib` after the server started advertising the
three Qt FBConfigs. The probe now requires all three (id 1 double-buffer,
id 2/3 SingleBuffer) to carry the X visual and `GLX_X_RENDERABLE`. Cold
and warm recapture on the same seed: `BXSUMMARY host-glx passed=26 failed=0`
with `configs=3 visual=0x1 xRenderable=1`.
