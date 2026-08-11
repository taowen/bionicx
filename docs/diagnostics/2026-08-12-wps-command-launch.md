# WPS child-command tracing

## Symptom

Writer can export a valid PDF, but the completion dialog's **Open File** action
reports that the PDF cannot be opened. The existing `popen` bridge only showed
a failing `bash` command, which was not enough to tell whether that command was
the document opener or unrelated desktop integration.

## Diagnostic layer

The WPS compatibility library now traces `execve`, `execv`, `execvp`,
`posix_spawn`, `posix_spawnp`, and `system` when
`BIONICX_TRACE_COMMANDS=1`. Successful `popen` calls retain their concise
one-line diagnostic and gain module-relative backtraces under the same opt-in
flag. Calls and arguments are forwarded unchanged; production profiles do not
set the flag.

## Finding

The retained trace in `evidence/wps-command-launch.log` identifies three
commands during startup and the failed Open File workflow:

- `gsettings ... tablet-mode` comes from `libkprometheus.so`;
- `pidof wpsupdate` also comes from `libkprometheus.so`;
- `bash  <office6>` comes from `libkso.so+0x29f5e7c` on a QtCore worker thread.

Disassembly at that last call site constructs `bash <QString> <application
directory>`. The QString is empty in this run. Embedded resources adjacent to
this code are desktop/MIME association scripts (`assocheck.sh`,
`desktopcheck.sh`, and `repair.sh`), so this command is a desktop-association
check rather than the PDF viewer launch itself.

No traced `exec*`, `posix_spawn*`, or `system` call occurs when Open File is
pressed. The next diagnostic boundary is therefore below or outside those
public glibc entry points (for example Qt's internal fork/syscall path), not a
reason to fabricate a `bash` result or bypass the error.

The untraced WPS compatibility probe was rerun after adding the wrappers. It
passed 21/21, including the Android-shell `popen` contract and X11 rendering;
the summary is retained at the end of the evidence file.
