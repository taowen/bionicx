# WPS file-operation trace and PPTX temporary path

## Symptom

Genuine AArch64 WPS Presentation could edit a title slide, but its Save dialog
ended with `Failed to save`. The final application profile was not changed to
work around the failure, and the application remained alive.

## Focused diagnostic mode

The WPS compatibility library now has an opt-in `BIONICX_TRACE_FILES=1` mode.
It records `mkstemp`/`mkstemps`/`mkostemp`/`mkostemps`, link, and rename calls.
Failed `fopen` calls include the caller module and ASLR-independent module
offset; an empty path additionally gets a short module-offset backtrace.

The wrappers restore the real call's `errno` and return value. They neither
retry nor rewrite a path. With the variable absent, the new successful-operation
traces and backtraces are disabled. This mode is diagnostic evidence, not part
of the final WPS profile.

## Decisive observation

On x300 `01408BH601027129`, API 34, the serializer called:

```text
wps-file-trace: mkstemp template=??/PreXXXXXXXX result=-1 errno=2 (No such file or directory)
wps-sysvipc-compat: fopen failed errno=2 path= caller=.../libkso.so+0x364dbe0
```

Changing `TMPDIR`, `TMP`, and `TEMP` did not change this relative template.
Changing the locale changed the malformed-looking prefix but did not make it a
usable directory. Creating a literal writable `office6/??` directory and
repeating the same save made the call succeed:

```text
wps-file-trace: mkstemp template=??/PreAHoShq result=26 errno=0 (success)
```

WPS then wrote `Presentation1.pptx` (36,593 bytes), and `unzip -t` accepted all
members. The temporary file was removed by WPS after serialization.

This isolates an application-specific relative temporary-directory assumption
inside the proprietary PPTX writer. A generic `mkstemp` path rewrite would hide
client bugs and change libc semantics, so the reproducible WPS installer will
create only the exact private directory the client expects.

The compact retained trace is `evidence/wps-presentation-save-trace.log`.
