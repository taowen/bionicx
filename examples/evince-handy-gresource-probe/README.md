# Evince libhandy GResource probe

Evince 48 aborts in `libhandy` `hdy_themes_update`:

```text
Handy:ERROR:../src/hdy-main.c:83:hdy_themes_update:
assertion failed: (hdy_resource_exists (resource_path))
```

`/sm/puri/handy/themes/shared.css` is compiled into `libhandy-1.so` as
`.gresource.hdy`. `patchelf --set-rpath` moves that section and
`g_resources_get_info` fails. ELF fixup already skips `.gresource`; this
library was rewritten before that skip.

```sh
ANDROID_SERIAL=<serial> examples/evince-handy-gresource-probe/install-and-run.sh
```

Expect `BXSUMMARY evince-handy-gresource passed=6 failed=0` (GTK/Handy
`dlopen`, `gtk-init`, shared/fallback CSS, `hdy_init` without assert).
