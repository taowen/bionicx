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
