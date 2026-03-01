# mkdir

Standalone Linux-native port of FreeBSD `mkdir` for Project Tick BSD/Linux Distribution.

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

- The FreeBSD utility was rewritten into a standalone Linux-native build instead of preserving BSD build glue, `err(3)`, or `setmode(3)` dependencies.
- Final directory creation maps to Linux `mkdir(2)`. When `-m` is present, the port follows FreeBSD semantics by applying the requested mode with `chmod(2)` after successful creation so the result is not truncated by `umask`.
- For symbolic `-m` values, `+` and `-` are interpreted relative to an initial `a=rwx` mode, matching `mkdir.1` rather than inheriting `chmod(1)`'s default-`umask` behavior.
- `-p` walks the path a component at a time with repeated `mkdir(2)` and `stat(2)` calls. Intermediate directories use a temporary `umask(2)` adjustment so Linux still grants owner write/search bits as required by POSIX and FreeBSD behavior.
- Mode parsing is local project code; no shared BSD compatibility shim is required.
- Path handling is dynamic and does not depend on `PATH_MAX`, `dirname(3)`, or in-place mutation of caller-owned argument strings.

## Supported / Unsupported Semantics

- Supported: FreeBSD/POSIX option surface `-m`, `-p`, and `-v`, including octal and symbolic modes.
- Supported: existing-directory success for `-p`, verbose output for newly created path components only, and correct failure when an existing path component is not a directory.
- Unsupported by design: GNU long options and GNU-specific parsing extensions. This port keeps BSD/POSIX command-line parsing strict.
- Linux-native limitation: kernel/filesystem specific permission inheritance such as SGID propagation follows the underlying Linux filesystem semantics.
