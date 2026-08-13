# Firefox ESR NSS profile database

## Symptom

Untraced Firefox ESR 140 on the shared seed reaches `https://example.com/`
and paints the `nssFailure2` page ("Personal Security Manager"). MOZ_LOG
`pipnss:5` reports:

```text
NSS profile at '.../homes/firefox-esr/online'
failed to initialize NSS with codes -8023 -8023
last-resort NSS_NoDB_Init
nsNSSComponent::InitializeNSS() failed
```

`-8023` is `SEC_ERROR_BAD_DATABASE`. `cert9.db` / `key4.db` are not created
in the profile. Pre-seeding those files from a working probe database does
not change the error.

## Controlled client

`examples/nss-ckbi-probe` is an AArch64 glibc client that dlopens the same
GreD `libnss3.so` Firefox loads. After a relative GreD symlink to the shared
`libnssckbi.so` (not a per-app copy):

```text
BXSUMMARY nss-ckbi passed=15 failed=0
```

That includes `NSS_Initialize("sql:...", NSS_INIT_OPTIMIZESPACE)`, 176 trust
anchors, and a TLS 1.3 handshake to `example.com:443` (resolved on this
device to `198.18.0.31`). OpenSSL from the same UID also verifies the peer
certificate. The gap is therefore inside Firefox's PSM initialization, not
missing roots, DNS, or the GreD NSS libraries.

Firefox `libsoftokn3.so` DT_NEEDED includes `libmozsqlite3.so`. The probe
loads that module through `$ORIGIN` when it `dlopen`s GreD `libsoftokn3.so`.

## Not claimed

Online Firefox navigation is not accepted. The sandbox banner about reduced
OS protection remains. `MOZ_DISABLE_CONTENT_SANDBOX`,
`network.process.enabled=false` and `MOZ_FORCE_DISABLE_E10S` did not make
`NSS_Initialize` succeed.
