# Evince 134 and libhandy `.gresource.hdy`

Untraced Evince 48 (with or without the PDF) aborted:

```text
Handy:ERROR:../src/hdy-main.c:83:hdy_themes_update:
assertion failed: (hdy_resource_exists (resource_path))
```

`/sm/puri/handy/themes/shared.css` is compiled into `libhandy-1.so.0`
as `.gresource.hdy`. An earlier `patchelf --set-rpath` moved that
section next to `.dynstr` (`VMA 0xd0048`). `g_resources_get_info` then
failed. ELF fixup already skips `.gresource`, but this library was
rewritten before that skip. `libgtk-3.so` had already been restored.

## Controlled client

`examples/evince-handy-gresource-probe` `dlopen`s `libhandy-1.so.0`,
looks up shared/fallback CSS, then calls `hdy_init()`. Before restore:

```text
BXTEST FAIL handy-shared-css The resource at “…/shared.css” does not exist
```

After `bxapt install --reinstall libhandy-1-0` the section is back at
`VMA 0x280` and RUNPATH is untouched:

```text
BXSUMMARY evince-handy-gresource passed=6 failed=0
```

Untraced Evince then stays up and renders `bionicx-pages.pdf` page 1
of 2.
