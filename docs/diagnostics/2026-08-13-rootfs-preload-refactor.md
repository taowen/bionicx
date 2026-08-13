# Rootfs preload refactor

## Decision

The package-manager spike made the old `android-tmp` name and single-file
implementation inaccurate. The module had acquired FHS file operations,
temporary-file APIs, PATH repair, helper execution, shebang parsing and
rootless ownership behavior.

The public compatibility name is now only `rootfs`. There is deliberately no
legacy alias or runtime fallback. Every tracked profile and the Android host
use `libbionicx-rootfs.so`.

## Internal boundaries

The preload remains one shared object because its interposed libc calls share
one path policy and must form one predictable `RTLD_NEXT` chain. Its source is
split into three compiled units:

| Unit | Responsibility |
| --- | --- |
| `rootfs-path.c` | FHS mapping, file APIs, `/tmp`, Unix pathname sockets |
| `rootfs-exec.c` | rootfs PATH, child helpers, real shebang interpreters |
| `rootfs-metadata.c` | chmod/chown mapping and Android app-UID ownership policy |

`rootfs-internal.h` contains only the private contract shared by those units.
Package resolution and apt configuration remain outside the preload.

`bxapt` was separately divided into argument parsing, DNS discovery,
app-private upload/state preparation, command construction and execution. It
continues to be a thin launcher for Debian apt/dpkg rather than a resolver.

## Regression gates

`tests/test-rootfs-compat.sh` builds the preload for the host and checks FHS
open mapping, `mkstemp`, rootless ownership handling, PATH search and shebang
execution. The Android APK gate checks that it contains
`libbionicx-rootfs.so` and no old module.

On x300 `01408BH601027129`, with the old device library removed:

- signed `bxapt update` exited 0;
- removing and reinstalling the seven-package `x11-apps` cohort exited 0,
  including maintainer scripts and `libc-bin` triggers;
- `apt check` exited 0;
- removing/reinstalling `hello`, `dpkg -V hello`, and executing it all exited
  0, with `Hello, world!` output;
- `xclock` launched with `LD_PRELOAD=libbionicx-rootfs.so` at 1920x1080, and
  screenshots three seconds apart differed in the seconds glyph.
