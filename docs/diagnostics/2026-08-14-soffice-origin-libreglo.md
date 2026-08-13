# LibreOffice `$ORIGIN` / `libreglo.so`

`soffice.bin` exited 127 with `libreglo.so: cannot open shared object file`
even though the file lives at `usr/lib/libreoffice/program/libreglo.so`.

Debian ships `usr/lib/aarch64-linux-gnu/libuno_cppuhelpergcc3.so.3` as a
symlink into `program/`. The normalized `RUNPATH` listed system directories
before `$ORIGIN`. The loader therefore opened the symlink; `$ORIGIN` expanded
to `aarch64-linux-gnu`, which does not contain `libreglo.so`.
`cppu::getUnoIniUri()` uses `dladdr` on that same open path and then looked
for `unorc` next to the symlink.

ELF fixup now records the real object's directory as a concrete `RUNPATH`
entry. It prepends that directory when every colliding system SONAME is only
a symlink back to the same file, and appends it when the directory also
ships a real system SONAME (WPS bundled FreeType). A previous append is
dropped on re-fixup so the prepend can take effect.

On device `01408BH601027129`, `soffice-origin-probe` is 5/5,
`soffice.bin --headless --version` prints `LibreOffice 25.2.3.2`, and
`--terminate_after_init` exits 0. Opening the Writer fixture (GUI or
`--convert-to`) still throws `WrappedTargetRuntimeException` inside
`libmergedlo.so`; that is a later document-load failure, not the ELF search.

No `LD_LIBRARY_PATH`, per-app library copy, or RPATH inheritance.
