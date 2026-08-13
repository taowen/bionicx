# Mozilla GreD NSS builtin-roots probe

Firefox ESR loads `libnssckbi.so` from `MOZILLA_FIVE_HOME`, not as a bare
SONAME. The Debian `firefox-esr` package ships bundled softoken without that
module. This probe requires the GreD path to resolve, `NSS_Init` to succeed,
and at least 40 trust anchors to be visible. The installer creates one
relative symlink from the shared `libnss3` copy; it does not copy libraries
into a profile.

```sh
ANDROID_SERIAL=<serial> examples/nss-ckbi-probe/install-and-run.sh
```
