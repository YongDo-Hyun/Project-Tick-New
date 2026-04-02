# rm

Standalone Linux-native port of FreeBSD `rm` for Project Tick BSD/Linux Distribution.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

## Test

```sh
gmake -f GNUmakefile test
gmake -f GNUmakefile test CC=musl-gcc
```

## Port Strategy

- The FreeBSD utility was rewritten into a standalone Linux-native build instead of preserving BSD build glue, `err(3)`, `fts(3)`, `lchflags(2)`, whiteout handling, or `SIGINFO` support.
- Regular file and symlink removal maps to Linux `unlinkat(2)` with flags `0`.
- Directory removal maps to Linux `unlinkat(2)` with `AT_REMOVEDIR`; recursive traversal uses `openat(2)`, `fdopendir(3)`, `readdir(3)`, `fstatat(2)`, and `unlinkat(2)` so traversal stays dynamic and does not depend on `PATH_MAX`.
- `-x` is implemented by comparing `st_dev` from `fstatat(2)` against the top-level operand device. The walk does not descend into entries that live on a different mounted filesystem.
- Interactive policy is Linux-native but manpage-aligned: `-i` prompts per operand, recursive directory descent is confirmed before traversal, and `-I` performs a single upfront confirmation when the operand set is large or includes recursive directory removal.

## Supported / Unsupported Semantics

- Supported: `rm` options `-d`, `-f`, `-i`, `-I`, `-P`, `-R`, `-r`, `-v`, and `-x`, plus `unlink [--] file`.
- Supported: strict rejection of `/`, `.`, and `..`; removal of symlinks themselves rather than their targets; `--` for dash-prefixed names; and verbose output on successful removals.
- Unsupported on Linux: `-W`. FreeBSD uses it for whiteout undelete semantics, but Linux does not expose an equivalent userland API; the port fails explicitly instead of silently degrading.
- Unsupported by design: GNU long options and GNU-specific option parsing extensions. This port keeps the FreeBSD/POSIX command-line surface strict.
