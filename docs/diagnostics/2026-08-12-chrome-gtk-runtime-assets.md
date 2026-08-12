# Chrome GTK runtime assets

Chrome loads GTK 3 with `dlopen()`, so the executable's ordinary `DT_NEEDED`
closure did not make the native Save Page dialog a reproducible part of the
Chrome bundle. The old device payload could also retain manually installed
probe files and hide omissions in a rebuilt bundle.

The Chrome builder now treats `libgtk-3.so.0` and all eleven ARM64 GDK Pixbuf
loaders as explicit ELF roots. It recursively resolves those roots, preserves
the requested plugin SONAME aliases, and generates `loaders.cache` by running
the ARM64 `gdk-pixbuf-query-loaders` tool in an ARM64 build container. The
cache is rewritten to the fixed Android app-private path and checked for build
host path leakage. The dependency lock gained only the three libraries exposed
by the TIFF loader closure: `liblzma5`, `libstdc++6`, and `libzstd1`.

The bundle also carries compiled GLib schemas, a generated shared-MIME database,
and cached Adwaita/hicolor icon themes below `${APP}/share`. Installation clears
only the selected profile's old immutable app payload before extracting the new
one.

On x300 `01408BH601027129`, Android 14/API 34, the untraced Chrome 151 profile
continued to use `--no-sandbox`, opened Example Domain, and opened its genuine
GTK Save Page dialog through Chrome's menu. The dialog rendered folder and file
icons, accepted the XI2-delivered Save click, and wrote:

```
    685 files/homes/chrome/Downloads/BionicX-GTK.html
 155941 files/homes/chrome/Downloads/example.com.html
```

Chrome and the containing Android process remained alive, and filtered logcat
contained no GTK, GDK, Pixbuf, SIGTRAP, or SIGSEGV error. The dialog and final
download state are retained in `evidence/chrome-gtk-runtime-assets-dialog.png`
and `evidence/chrome-gtk-runtime-assets-saved.png`.
