# Chrome multiprocess without FD-ownership abort

`--no-sandbox` alone did not keep GPU/renderer children alive. They
aborted at `ChromeMain` with `Crashing due to FD ownership violation`.

That string is Chrome's own `close()` interposer in
`base/files/scoped_file_linux.cc`. After
`EnableFDOwnershipEnforcement`, a second `ScopedFD` acquire or a
`close()` of an owned fd aborts. The table is process-local and empty
after `exec`, so inherited Activity descriptors are not the trigger.
Closing FDs ≥ 3 in `bionicx-exec` before the first glibc exec was
tried on vivo `10AFA31610002QH` and still produced 212 ownership
aborts.

`--disable-crashpad-for-testing` on the browser argv is not forwarded
to `--type=` children. Mutating every `execve` in `fhs-exec.c` to
inject it hid the abort, but that is a Chrome special case in the
generic FHS interposer and must not stay there. Later `execve`s also
carry Mojo remaps, so the interposer must not close FDs either.

Chrome Linux `ChromeMain` reads `CHROME_EXTRA_FLAGS` and appends those
switches before `ContentMain` / Crashpad init, in every process
including children that inherit the environment. Both Chrome profiles
set `CHROME_EXTRA_FLAGS=--disable-crashpad-for-testing`. `fhs-exec.c`
stays Chrome-agnostic. `bionicx-exec` still closes inherited Activity
FDs as hygiene.

vivo `10AFA31610002QH`: smoke `about:blank` paints with zygote/renderer
children. Untraced `chrome-vulkan.json` without `--single-process`
reports `WEBGL_OK` on ANGLE Vulkan / Vortek (Mali-G1-Ultra MC12). See
`evidence/vivo-10AFA31610002QH/chrome-smoke.png` and `chrome-vulkan.png`.
