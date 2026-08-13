# Firefox ESR online navigation

Runs `nss-ckbi-probe` (GreD softoken + TLS to example.com), then the
untraced `firefox-esr-online` profile, then a force-stop cold start.
Expect `BXSUMMARY nss-ckbi passed=… failed=0`, a live Example Domain
frame, `cert9.db`/`key4.db`, and the same page after cold start.

```sh
ANDROID_SERIAL=<serial> examples/firefox-online/run-online.sh
```
