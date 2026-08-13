# Shadow account-file open contract

Debian `groupadd` from `uuid-runtime` postinst failed on the clean seed with:

```text
groupadd: cannot open /etc/group: Read-only file system
```

The host `runtime-contract-probe` already covered ordinary `open`/`fopen` and
`link`/`linkat` locks. That was not enough. `groupadd` imports glibc's
fortified `__open_2` and `lckpwdf`, both of which open `/etc/group` or
`/etc/.pwd.lock` through libc internals and never reach the interposed
`open`/`fopen` symbols. Those calls hit Android's read-only `/etc`.

The shared runtime now interposes `__open_2`, `__open64_2`, `__openat_2`,
`__openat64_2`, `lckpwdf` and `ulckpwdf`, and path redirection uses the
`BIONICX_ROOTFS` captured when the contract library is loaded. `execve` also
re-injects the runtime-owned environment so dpkg's sanitized helper
environment cannot drop the contract.

## Probes

Host `tests/test-runtime-contract.sh` now writes `/etc/group` through `fopen`,
opens it with `__open_2`, and locks it with `lckpwdf`, including after
`unsetenv(BIONICX_ROOTFS)`.

The device client `examples/account-file-probe` is a genuine AArch64 glibc
binary. It is installed without replacing the shared seed and ran untraced as
the ordinary `io.taowen.bx` UID on `01408BH601027129`:

```text
BXTEST PASS fopen-group-rw
BXTEST PASS fortified-open-group
BXTEST PASS lckpwdf
BXTEST PASS captured-rootfs-after-unsetenv
BXSUMMARY account-file 4/4
```

## Device package result

`bxapt install uuid-runtime` then configured without `systemd-sysusers`
pre-creation:

```text
ii  adduser       3.152
ii  uuid-runtime  2.41-5
uuidd:x:100:101::/run/uuidd:/usr/sbin/nologin
uuidd:x:101:
```

`tests/test-bxapt-transaction-lifecycle.sh` exercised install, failed
configure, recover, remove and autoremove from that seed. `dpkg --audit` stayed
empty after every final state, apt marks kept `uuid-runtime` manual only while
installed, and the ELF ledger returned to 1010 entries after removal.

```text
bxapt transaction lifecycle: PASS
```
