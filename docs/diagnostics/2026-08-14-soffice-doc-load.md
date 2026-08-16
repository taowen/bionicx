# LibreOffice Writer document load

After the `$ORIGIN`/`libreglo` fix, `soffice.bin --terminate_after_init`
succeeded but opening a Writer document — GUI or `--convert-to` — aborted
with `com::sun::star::lang::WrappedTargetRuntimeException`.

Three shared-seed gaps stacked:

1. `bxapt` installs without Recommends, so `libreoffice-gtk3` and
   `libreoffice-math` were missing. The Writer profile asks for
   `SAL_USE_VCLPLUGIN=gtk3`; without `libvclplug_gtk3lo.so` there is no
   document VCL.
2. First document creation calls `dp_misc::syncRepositories`. The seed has
   an empty `share/extensions` tree; that sync throws. Setting the official
   bootstrap variable `DISABLE_EXTENSION_SYNCHRONIZATION=1` skips it.
3. `soffice.bin` exits 81 to ask the Debian wrapper to restart after
   first-run setup. The profile now runs `${RUNTIME}/bin/sh` on
   `${RUNTIME}/usr/bin/soffice` so that restart happens.

`soffice-origin-probe` now asserts gtk3/generic VCL plugins and
`dlopen(libswlo.so)` without forking `--convert-to`. The former
`soffice-doc-probe` was 8/8 on `01408BH601027129`: gtk3 plugin present,
`libswlo.so` loads, `--convert-to txt:Text` writes the fixture heading.
Untraced Writer opens `bionicx-writer.odt` and shows
`LibreOffice Writer on BionicX`.

No `LD_LIBRARY_PATH`, per-app library copy, or PRoot.
