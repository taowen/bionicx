# LibreOffice `$ORIGIN` / `libreglo.so` probe

`soffice.bin` fails at load with `libreglo.so: cannot open shared object file`
even though the file lives next to it in `usr/lib/libreoffice/program/`.

Debian ships `usr/lib/aarch64-linux-gnu/libuno_cppuhelpergcc3.so.3` as a
symlink into `program/`. The normalized `RUNPATH` lists system directories
before `$ORIGIN`. The loader therefore opens the symlink; `$ORIGIN` expands
to `aarch64-linux-gnu`, which does not contain `libreglo.so`, and
`getUnoIniUri()` looks for `unorc` next to the symlink. ELF fixup records the
real object's directory as a concrete `RUNPATH` entry and prepends it when
every colliding system SONAME is only a symlink back to that file.

```sh
ANDROID_SERIAL=<serial> examples/soffice-origin-probe/install-and-run.sh
```

Expect `BXSUMMARY soffice-origin passed=8 failed=0`, gtk3/generic VCL
plugins present, `dlopen(libswlo.so)`, `soffice.bin --version` to print a
LibreOffice banner, and `--terminate_after_init` to exit 0. The probe must
not replace the shared seed and must not fork a convert-to Writer session.
