# KeePassXC CLI database probe

Creates a key-file-only `.kdbx` on the shared seed with Debian
`keepassxc-cli`, adds one entry, lists and shows it, then reopens the
same file. That is the durable database path the GUI later unlocks.
Expect `BXSUMMARY keepassxc-cli passed=6 failed=0`.

The GUI profile maps KeePassXC first, then opens the same fixture over
D-Bus (`keepassxc-deferred-open`). Passing `--keyfile` on the keepassxc
command line constructs `DatabaseWidget` before `bringToFront()` and
dies in `QWidgetPrivate::showChildren` (NULL `d_ptr`).

```sh
ANDROID_SERIAL=<serial> examples/keepassxc-cli-probe/install-and-run.sh
```
