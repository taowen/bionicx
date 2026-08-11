# App-private glibc UTF-8 locale

## Symptom

`setlocale(LC_ALL, "C.UTF-8")` failed in the minimal runtime. The runtime had
only shared libraries; Android has no host glibc locale tree at the absolute
paths expected by a desktop distribution.

## Correction

The common integration runtime now copies Winlator's matching glibc 2.39
`en_US.utf8` locale data. The runtime profile declares:

```text
LANG=en_US.UTF-8
LOCPATH=${RUNTIME}/usr/lib/locale
```

The test consumes the environment with `setlocale(LC_ALL, "")` and verifies
`nl_langinfo(CODESET)` is UTF-8. This retains real locale behavior instead of
silently downgrading applications to the plain `C` locale.

## x300 regression

```text
BXTEST PASS locale-utf8
BXSUMMARY runtime passed=19 failed=1
```

Additional locales can be installed as bundle data without changing the
executor. Application profiles should explicitly select the locale closure
they ship.
