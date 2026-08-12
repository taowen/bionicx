# IceWM taskbar and runtime-loaded Imlib2 plugins

Enabling `ShowTaskBar=1` made unmodified Debian ARM64 IceWM 3.7.4 terminate
with SIGSEGV before it could manage the controlled application windows. The
same failure occurred without Frida or any other instrumentation.

An app-private ARM64 gdbserver launch on x300 `01408BH601027129` caught the
fault at `strcmp` with a null first argument. Debian debuginfod resolved the
caller stack to:

```
strcmp
__imlib_LookupLoaderByModulePath(file=NULL) at loaders.c:173
__imlib_LookupKnownLoader
__imlib_FindBestLoader
__imlib_LoadImage(.../icons/icewm_16x16.png)
```

The bundle contained `xpm.so`, but not Imlib2's runtime-loaded `png.so`.
Recursive `DT_NEEDED` scanning cannot discover a plugin selected later by file
format. The bundle builder now declares both plugin ELFs as dependency resolver
entries, checks that they exist, and copies their transitive libraries. This is
bundle metadata rather than an Imlib2 or IceWM patch.

The ordinary untraced app-UID profile now enables the panel and verifies the
mapped, screen-width taskbar in the X hierarchy before launching two clients:

```
BXTEST PASS icewm-manager-start
BXICEWM taskbar window=0x400061 geometry=1920x32+0+0
BXTEST PASS icewm-taskbar-mapped
BXTEST PASS icewm-two-clients first=1 second=1
BXSUMMARY icewm passed=3 failed=0
icewm-probe exited with 0
```

The core X11 suite remains green at 18/18. Visual inspection confirms the panel
background at the bottom of the Android surface, but its child controls and
text are not yet completely painted. This milestone therefore proves plugin
closure and panel window lifecycle, not a finished desktop shell.

The next diagnosis found fourteen Render minor 4 failures. IceWM supplied a
nonzero 1-bit `CPClipMask` while creating pictures for themed assets. BionicX
now retains that pixmap and clip origin, validates its depth, applies it during
fill/composite/glyph operations, and clears it when rectangle clips replace the
mask. The controlled Render probe verifies an 8x8 mask by reading back a red
inside pixel and unchanged blue outside pixel. IceWM no longer emits any Render
`BadImplementation` errors. The still-incomplete visual panel is consequently
a separate nested-window composition issue rather than this Render request.
