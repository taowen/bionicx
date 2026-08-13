# LibreOffice Writer document-load probe

`soffice.bin --terminate_after_init` succeeds after the `$ORIGIN`/`libreglo`
fix, but opening the Writer fixture — GUI or `--convert-to` — throws
`com::sun::star::lang::WrappedTargetRuntimeException`.

The Writer profile sets `SAL_USE_VCLPLUGIN=gtk3`. Debian `libreoffice-writer`
does not pull in `libreoffice-gtk3` under `bxapt`'s `--no-install-recommends`,
so `libvclplug_gtk3lo.so` is missing. Instantiating a document then runs
`dp_misc::syncRepositories`, which throws
`WrappedTargetRuntimeException`. The profile disables extension
synchronization (no bundled extensions on this seed) and launches the
Debian `soffice` wrapper so exit 81 restarts after first-run setup.

```sh
ANDROID_SERIAL=<serial> examples/soffice-doc-probe/install-and-run.sh
```

Expect `BXSUMMARY soffice-doc passed=8 failed=0` and a `txt:Text` conversion
that contains `LibreOffice Writer on BionicX`. The probe must not replace the
shared seed.
