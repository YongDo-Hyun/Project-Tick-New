# realpath

Standalone Linux-native port of FreeBSD `realpath(1)` for Project Tick BSD/Linux Distribution.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

Binary output:

```sh
out/realpath
```

## Test

```sh
gmake -f GNUmakefile test
gmake -f GNUmakefile test CC=musl-gcc
```

## Port Strategy

- The port follows the standalone sibling layout used by `bin/echo` and `bin/pwd`: self-contained `GNUmakefile`, no BSD build glue, and a shell test suite that validates stdout, stderr, and exit status.
- The original FreeBSD code used a fixed `PATH_MAX` stack buffer and BSD libc helpers from `<err.h>`. This port removes those dependencies and uses dynamically allocated `realpath(3)` results plus local error-reporting helpers.
- Path resolution intentionally remains `realpath(3)`-based because `realpath.1` is explicit: the utility resolves paths exactly as the C library canonicalization routine does on Linux.

### API Mapping

| BSD dependency | Linux / musl replacement | Rationale |
|---|---|---|
| `<sys/param.h>` / `PATH_MAX` stack buffer | `realpath(path, NULL)` | Avoids fixed path limits and is supported by glibc and musl on Linux |
| `<err.h>` `warn(3)` | local `fprintf(3)`-based diagnostics | No BSD libc helper dependency |
| `__dead2` | plain `static void usage(void)` + `exit(1)` | Portable C17 |
| FreeBSD libc canonicalization call with caller buffer | Linux libc canonicalization with allocated buffer | Same semantics required by `realpath(1)` manpage, safer memory model |

## Supported Semantics On Linux

- `realpath [path ...]` and `realpath -q [path ...]`
- Default operand `.` when no path is given
- Mixed success and failure across multiple operands: successful resolutions are printed, any failure makes the exit status non-zero
- Diagnostics suppressed only for path-resolution failures under `-q`
- `--` ends option parsing, so `realpath -- -name` treats `-name` as an operand
- Internal fixed-size path buffers were removed, but Linux kernel/libc pathname resolution may still fail with `ENAMETOOLONG` on extremely long inputs; this is reported explicitly rather than truncated or stubbed

## Intentionally Unsupported Semantics

- GNU `realpath` extensions such as `-e`, `-m`, `--relative-to`, `--strip`, `--canonicalize-missing`, `--help`, and `--version` are not implemented. FreeBSD `realpath.1` only defines `-q`.
- Nonexistent paths are not canonicalized. The utility follows `realpath(3)` and fails with the native Linux libc error instead of inventing GNU-style missing-path behavior.
- Linux-specific mount namespace or procfs tricks are not exposed as options because `realpath.1` defines no such interface.
