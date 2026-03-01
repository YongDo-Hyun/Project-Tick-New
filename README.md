# ls

Standalone musl-libc-based Linux port of FreeBSD `ls` for Project Tick BSD/Linux Distribution.

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

- The port aims to preserve POSIX `ls` behavior first, then FreeBSD/BSD `ls` behavior where Linux can represent it without fake compat layers.
- BSD-only interfaces were removed instead of wrapped: traversal uses direct Linux/POSIX `opendir(3)`/`readdir(3)`/`stat(2)`/`lstat(2)` logic, and birth-time handling uses Linux `statx(2)` via the raw syscall interface.
- GNU libc-only helpers are avoided in the implementation. `-v` uses an in-tree natural version comparator instead of `strverscmp(3)`.
- Long-format identity lookups use `getpwuid_r(3)` and `getgrgid_r(3)` with dynamically sized buffers so the port stays musl-clean.
- The bundled `ls.1` is the contract for this port. FreeBSD-only `-o`, `-W`, and `-Z` remain explicit hard errors on Linux instead of being emulated.

## Linux Semantics

Supported mappings:

- Normal metadata comes from `stat(2)` / `lstat(2)`.
- `-H`, `-L`, `-P` control command-line or full symlink following on Linux.
- `-U` uses Linux `statx(2)` birth time when available and falls back to `mtime` when the kernel or backing filesystem does not expose creation time.
- `--color` and `-G` use fixed ANSI color sequences only; no termcap or `LSCOLORS` parser is required.
- `LS_SAMESORT` is honored like `-y`.

Intentionally unsupported:

- `-o`: FreeBSD file flags are not portable on Linux and are not emulated.
- `-W`: whiteout entries do not exist in Linux VFS.
- `-Z`: FreeBSD MAC label output has no portable Linux equivalent in this tool.

Test scope:

- The standalone shell suite fixes `LC_ALL=C` for deterministic sorting and covers usage/error paths, BSD visibility rules (`-a`, `-A`, root `-I` handling), default and explicit symlink policies (`-H`, `-L`, `-P`), recursive traversal, size/time/version sorting, `-y`/`LS_SAMESORT`, long-format output, block totals, column/stream layouts, directory grouping, color toggles, and explicit unsupported-option failures.
