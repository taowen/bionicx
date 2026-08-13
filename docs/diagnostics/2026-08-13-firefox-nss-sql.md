# Firefox ESR NSS profile database

## Symptom

Untraced Firefox ESR 140 reached `https://example.com/` and painted the
`nssFailure2` page. MOZ_LOG `pipnss:5` reported:

```text
NSS profile at '.../homes/firefox-esr/online'
failed to initialize NSS with codes -8023 -8023
last-resort NSS_NoDB_Init
nsNSSComponent::InitializeNSS() failed
```

`-8023` is `SEC_ERROR_PKCS11_DEVICE_ERROR` (a PKCS#11 module returned
`CKR_DEVICE_ERROR`), not a missing trust-anchor file. `cert9.db` / `key4.db`
were not created.

## Cause

Firefox maps GreD `libnss3.so` first, then `PR_LoadLibrary("libsoftokn3.so")`.
The interposed bare-SONAME `dlopen` searched `$BIONICX_ROOTFS` multiarch
before the already-mapped GreD directory, so PSM got the Debian
`libsoftokn3.so` (a different NSS build) instead of
`/usr/lib/firefox-esr/libsoftokn3.so` (`DT_NEEDED` `libmozsqlite3.so`).

## Controlled client

`examples/nss-ckbi-probe` now follows that path: `dlopen` GreD `libnss3.so`,
then the bare SONAME `libsoftokn3.so` must resolve under `firefox-esr`,
`NSS_Initialize("sql:...", NSS_INIT_NOROOTINIT|NSS_INIT_OPTIMIZESPACE)`,
`PK11_GetInternalKeySlot` / `PK11_InitPin`, 176 trust anchors, and TLS to
`example.com:443`. Host `tests/test-runtime-contract.sh` places two
`libsoftokn3.so` copies and requires the GreD marker after `libnss3.so` is
mapped.

```text
BXSUMMARY nss-ckbi passed=17 failed=0
```

The runtime searches the directory of any already-mapped `libnss3.so` (via
`dl_iterate_phdr`) and `MOZILLA_FIVE_HOME` before multiarch.

## Device result

After the runtime change, the same untraced profile created `cert9.db` /
`key4.db` and rendered Example Domain with a lock icon. The sandbox banner
about reduced OS protection remains. Seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.
