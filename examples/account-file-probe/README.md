# Account-file contract probe

Controlled glibc client for the shadow-tools account-file path. It writes
`/etc/group` through `fopen`, the fortified `__open_2` entry used by Debian
`groupadd`, glibc `lckpwdf()`, the libaudit stubs `groupadd` calls, and the
f2fs copy-lock `st_nlink == 2` contract. It checks that every file lands in the
app-private rootfs. It does not replace the shared seed.

```sh
ANDROID_SERIAL=<serial> examples/account-file-probe/install-and-run.sh
```
