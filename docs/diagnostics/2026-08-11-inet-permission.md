# Android INTERNET permission for glibc AF_INET sockets

## Symptom

The runtime probe failed at its first `socket(AF_INET, SOCK_STREAM, 0)` with
`EPERM`; Unix-domain sockets and SCM_RIGHTS already passed.

## Root cause and correction

BionicX did not request Android's normal `android.permission.INTERNET` manifest
permission. Linux networking syscalls made by the hosted glibc process still
run under the APK UID and Android permission model. The permission was added to
the manifest; it is install-time and does not require a runtime dialog.

## x300 regression

The unchanged glibc client then completed socket, loopback bind, getsockname,
listen, connect, accept and byte transfer:

```text
BXTEST PASS tcp-loopback connect
BXSUMMARY runtime passed=18 failed=2
```

This establishes local AF_INET mechanics. DNS, TLS and external HTTP remain
separate Chrome-oriented integration checks.
