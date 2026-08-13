# Shadow-tools lock path contract

The clean `cups-daemon cups-client` transaction reproduced shadow-tools
failing with:

```text
groupadd: /etc/group.lock: No such file or directory
groupadd: cannot lock /etc/group; try again later.
```

The package runs chrootless under the BionicX virtual root. Its account-file
lock is created with the POSIX hard-link path (`link`/`linkat`), while the
existing FHS preload redirected ordinary file operations but not those two
calls. The host rootfs therefore had `/etc/group` but the lock operation still
targeted Android's real `/etc`.

The shared runtime now redirects both source and destination paths for
`link()` and `linkat()`, using the same `BIONICX_ROOTFS` mapping as `open`,
`rename`, and `unlink`. No package-specific script or pre-created account is
needed.

Regression coverage in `tests/test-runtime-contract.sh` creates and removes
`/etc/group.lock` through both APIs and verifies the backing file exists only
under the private rootfs. Results:

```text
runtime contract probe: PASS
rootfs ELF fixup: PASS
```

The next device acceptance run should start from a clean seed and require
`bxapt install cups-daemon cups-client` to finish without the prior
`systemd-sysusers` recovery step.
