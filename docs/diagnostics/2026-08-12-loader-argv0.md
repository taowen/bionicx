# glibc loader-mode argv0 handoff

## Chrome symptom

The first Google Chrome ARM64 launch used loader mode with profile
`argv0=chrome`. `bionicx-exec` incorrectly replaced the loader's executable
argument at array index 3, so glibc attempted to open a file literally named
`chrome` and exited 127 before dependency resolution.

## Correct contract

When `ld-linux-aarch64.so.1` is invoked explicitly, the target path is part of
the loader command line and must never be replaced. glibc 2.39 exposes
`--argv0 STRING`; the executor now emits:

```text
ld-linux-aarch64.so.1 --library-path PATH --argv0 NAME TARGET ARG...
```

Direct mode retains its existing behavior of replacing target `argv[0]`
inside the array passed to `execv`.

## Controlled and real-client evidence

The runtime probe has an opt-in `BIONICX_EXPECT_ARGV0` assertion. The dedicated
loader profile requires the non-path value `bionicx-controlled-argv0`; all 21
runtime/X11 checks passed and the session drained normally. The compact run is
in `evidence/loader-argv0-probe.log`.

Chrome then advanced past the malformed handoff and reported the first actual
dependency boundary, missing `libnspr4.so`. This proves that the intended
282,660,232-byte ARM64 Chrome ELF, rather than the argv0 string, reached glibc's
dependency loader.

