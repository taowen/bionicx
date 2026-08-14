# Chrome multiprocess without FD-ownership abort

`--no-sandbox` alone did not keep GPU/renderer children alive. They
aborted at `ChromeMain` with `Crashing due to FD ownership violation`
because Crashpad started in the child and Chrome's fixed FD-ownership
table rejected Android's inherited descriptors. The browser switch
`--disable-crashpad-for-testing` was not forwarded.

`libbionicx-runtime.so` now appends that switch on `execv`/`execve`/
`execvp`/`posix_spawn` when argv already contains `--type=` and does
not already have the switch.

vivo `10AFA31610002QH`: smoke `about:blank` paints with zygote/renderer
children and zero ownership lines. Untraced `chrome-vulkan.json` without
`--single-process` reports `WEBGL_OK` on ANGLE Vulkan / Vortek
(Mali-G1-Ultra MC12). See `evidence/vivo-10AFA31610002QH/chrome-smoke.png`
and `chrome-vulkan.png`.
