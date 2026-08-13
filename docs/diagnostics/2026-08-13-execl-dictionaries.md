# Perl execl("/bin/sh") must hit the rootfs shell

`dictionaries-common` postinst runs `ispell-autobuildhash`, which creates its
temporary directory with a Perl backtick. Perl 5.40 uses `execl("/bin/sh")`.
libc's `execl` called the kernel path directly, so Android's Bionic `sh` was
started with `LD_PRELOAD=libbionicx-runtime.so` and failed:

```text
CANNOT LINK EXECUTABLE "sh": library "libc.so.6" not found
```

The runtime now interposes `execl`, `execle` and `execvpe`.
`tests/test-runtime-contract.sh` checks that `execl("/usr/bin/bionicx-script")`
runs the rootfs script. After uploading that runtime, `dpkg --configure -a`
configured `dictionaries-common`, `mousepad` and `evince`.
