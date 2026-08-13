# KeePassXC CLI database probe

Creates a key-file-only `.kdbx` on the shared seed with Debian
`keepassxc-cli`, adds one entry, lists and shows it, then reopens the
same file. That is the durable database path the GUI later unlocks.
Expect `BXSUMMARY keepassxc-cli passed=6 failed=0`.

```sh
ANDROID_SERIAL=<serial> examples/keepassxc-cli-probe/install-and-run.sh
```
