# chmod

Standalone musl-libc-based Linux port of FreeBSD `chmod` for Project Tick BSD/Linux Distribution.

## Status

Project layout is detached from the top-level FreeBSD build.
It builds as a standalone Linux-native utility.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

## Test

```sh
gmake -f GNUmakefile test
```

## Notes

- Mode parsing lives in local project code, not a shared compat layer.
- The current implementation keeps the original recursive traversal logic.
- Recursive walking no longer depends on `fts(3)`, so it builds with musl.
- Verified with `gmake -f GNUmakefile clean test` and `gmake -f GNUmakefile clean test CC=musl-gcc`.
