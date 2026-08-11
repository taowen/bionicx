# Chrome child Crashpad FD ownership loop

## Symptom

After Chrome's browser window became stable, its network utility process exited
and restarted roughly 35 times per second with `Crashing due to FD ownership
violation`. Splitting stdout and stderr and removing the earlier variadic
`syscall` interposer did not change the rate. The same failure reproduced in the
`run-as` domain, both with and without any BionicX preload, excluding Android
seccomp and the compatibility module as causes.

## Localization

The PIE stack was normalized against exported `ChromeMain` at ELF virtual
address `0x0a5984ec`. Stable frames included `0x0f1d6a3c`, whose disassembly is
the failure branch after an atomic duplicate acquisition in Chromium's fixed
FD-ownership table. Nearby string references (`crashpad-handler-pid`,
`chrome_crashpad_handler`, and Crashpad monitor arguments) localized the caller
to child Crashpad initialization rather than networking.

The browser command line contained `--disable-crashpad-for-testing`, while its
spawned `--type=utility` command line did not. Chromium consumes this testing
switch in the browser and does not normally forward it to subprocesses.

## Correction

`libbionicx-chrome.so` interposes the fixed-signature glibc `execvp` API. When,
and only when, the new argv contains a Chrome `--type=` argument and does not
already contain the switch, it appends `--disable-crashpad-for-testing`. The
module resolves the real function in its constructor before Chrome forks. It
does not alter `close`, reset ownership state, or disable Chromium's ownership
enforcement.

On x300 the module propagated the browser policy to three children. During the
following 15-second observation window both ownership violations and network
service restarts fell from hundreds to zero. The real browser window and parent
process remained alive. This proves removal of the restart loop, not yet actual
HTTP/TLS navigation; fonts, D-Bus fallbacks, XFixes behavior, and navigation are
the next acceptance layers. See `evidence/chrome-child-crashpad-policy.log`.
