# Firefox ESR online navigation and cold start

Device `01408BH601027129` has working connectivity (`ping example.com`
replies). Seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.

`examples/nss-ckbi-probe` is 17/17: GreD `libsoftokn3.so`, 176 trust
anchors, and TLS to `example.com:443`. The untraced
`firefox-esr-online` profile then opened `https://example.com/`. The
compositor shows Example Domain with a lock icon in the URL bar. The
profile wrote `cert9.db` / `key4.db`. After `am force-stop` and a cold
start the same page and NSS databases are still there. `places.sqlite`
contains `https://example.com/`.

The sandbox banner about reduced OS protection remains (`--no-sandbox`
equivalent MOZ flags). No VirGL.

Evidence: `evidence/rebuild-2026-08-14/nss-ckbi-probe.log`,
`firefox-esr-online.log`, `firefox-esr-online.png`,
`firefox-esr-online-cold.png`.
