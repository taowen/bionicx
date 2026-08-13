# WPS on the identity-fixed seed, print to bionicx-test

Device `01408BH601027129` kept seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.
`bxapt install xdg-utils` then the hash-pinned
`wps-office_11.1.0.11720_arm64.deb`
(`172d8bcf3e3bc756994ae5adf66f63f22691e1ab0d18986c50bb6b6ab7f62948`)
configured `wps-office` `11.1.0.11720 ii`. `dpkg --audit` is empty.

`profiles/wps-office.json` requests `hostServices: ["cups"]`. The
controlled `bionicx-test` file destination accepted `lp` job
`bionicx-test-3`, listed by `lpstat -W completed`. The WPS GUI print
dialog is not claimed here.

Host `tests/test-wps-office-profile.sh` checks the cups host service
and the pinned deb hash in the WPS README.
