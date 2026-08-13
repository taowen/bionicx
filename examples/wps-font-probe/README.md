# WPS Fontconfig family coverage probe

Resolves Liberation Sans/Serif first (Latin plus formula operators
`± × π √ ∑`), then the Microsoft names WPS checks (Calibri, Cambria, Arial,
Times New Roman). Aliases to Liberation are installed only after that
coverage is present; proprietary fonts are not bundled. Expect
`BXSUMMARY wps-font-families passed=6 failed=0`.

```sh
ANDROID_SERIAL=<serial> examples/wps-font-probe/install-and-run.sh
```
