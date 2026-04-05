# Corebinutils — Overview

## What Is Corebinutils?

Corebinutils is Project Tick's collection of core command-line utilities, ported
from FreeBSD and adapted for Linux with musl libc. It provides the foundational
user-space programs that every Unix system needs — file manipulation, process
control, text processing, and system information tools — built from
battle-tested FreeBSD sources rather than GNU coreutils.

The project targets a clean, auditable, BSD-licensed alternative to the GNU
toolchain. Every utility compiles against musl libc by default, producing
statically-linkable binaries with minimal dependencies.

## Heritage and Licensing

All utilities derive from FreeBSD's `/usr/src/bin/` tree, carrying BSD
3-Clause or BSD 2-Clause licenses from the original Berkeley and FreeBSD
contributors. Project Tick's modifications (Copyright 2026) maintain the same
licensing terms. No GPL-licensed code is present in the tree.

The copyright headers trace a direct lineage:

```
Copyright (c) 1989, 1993, 1994
    The Regents of the University of California.  All rights reserved.
Copyright (c) 2026
    Project Tick. All rights reserved.
```

Key contributors acknowledged across the codebase include Keith Muller (dd),
Andrew Moore (ed), Michael Fischbein (ls), Ken Smith (mv), and Lance Visser
(dd).

## Design Philosophy

### Linux-Native, Not Compatibility Layers

Unlike many BSD-to-Linux ports that ship a compatibility shim library,
corebinutils rewrites platform-specific code using native Linux APIs:

- **`/proc/self/mountinfo`** replaces BSD `getmntinfo(3)` in `df`
- **`statx(2)`** replaces BSD `stat(2)` for birth time in `ls`
- **`sched_getaffinity(2)`** replaces BSD `cpuset_getaffinity(2)` in `nproc`
- **`sethostname(2)` from `<unistd.h>`** replaces BSD kernel calls in `hostname`
- **`prctl(PR_SET_CHILD_SUBREAPER)`** replaces BSD `procctl` in `timeout`
- **`fdopendir(3)` + `readdir(3)`** replaces BSD FTS functions in `rm`

### musl-First Toolchain

The build system preferentially selects musl-based compilers. The configure
script tries, in order:

1. `musl-clang`
2. `clang --target=<arch>-linux-musl`
3. `clang --target=<arch>-unknown-linux-musl`
4. `musl-gcc`
5. `clang` (generic)
6. `cc`
7. `gcc`

If a glibc toolchain is detected, configure refuses to proceed unless
`--allow-glibc` is explicitly passed.

### No External Dependencies

Core utilities have zero runtime dependencies beyond libc. Optional features
(readline in `csh`, crypto in `ed`) probe for system libraries at configure
time but degrade gracefully when absent.

## Complete Utility List

### File Operations

| Utility   | Description                          | Complexity | Source Files |
|-----------|--------------------------------------|------------|-------------|
| `cat`     | Concatenate and display files        | Simple     | 1 `.c`      |
| `cp`      | Copy files and directory trees       | Medium     | 3+ `.c`     |
| `dd`      | Block-level data copying/conversion  | Complex    | 8+ `.c`     |
| `ln`      | Create hard and symbolic links       | Medium     | 1 `.c`      |
| `mv`      | Move/rename files and directories    | Medium     | 1 `.c`      |
| `rm`      | Remove files and directories         | Medium     | 1 `.c`      |
| `rmdir`   | Remove empty directories             | Simple     | 1 `.c`      |

### Directory Operations

| Utility     | Description                        | Complexity | Source Files |
|-------------|------------------------------------|------------|-------------|
| `ls`        | List directory contents            | Complex    | 5+ `.c`     |
| `mkdir`     | Create directories                 | Medium     | 2 `.c`      |
| `pwd`       | Print working directory            | Simple     | 1 `.c`      |
| `realpath`  | Canonicalize file paths            | Simple     | 1 `.c`      |

### Permission and Attribute Management

| Utility     | Description                        | Complexity | Source Files |
|-------------|------------------------------------|------------|-------------|
| `chmod`     | Change file permissions            | Medium     | 2 `.c`      |
| `chflags`   | Change file flags (BSD compat)     | Medium     | 4 `.c`      |
| `getfacl`   | Display file ACLs                  | Medium     | 1 `.c`      |
| `setfacl`   | Set file ACLs                      | Medium     | 1 `.c`      |

### Process Management

| Utility     | Description                        | Complexity | Source Files |
|-------------|------------------------------------|------------|-------------|
| `kill`      | Send signals to processes          | Medium     | 1 `.c`      |
| `ps`        | List running processes             | Complex    | 6+ `.c`     |
| `pkill`     | Signal processes by name/attribute | Medium     | 1+ `.c`     |
| `pwait`     | Wait for process termination       | Simple     | 1 `.c`      |
| `timeout`   | Run command with time limit        | Medium     | 1 `.c`      |

### Text Processing

| Utility   | Description                          | Complexity | Source Files |
|-----------|--------------------------------------|------------|-------------|
| `echo`    | Write arguments to stdout            | Simple     | 1 `.c`      |
| `ed`      | Line-oriented text editor            | Complex    | 10+ `.c`    |
| `expr`    | Evaluate expressions                 | Medium     | 1 `.c`      |
| `test`    | Conditional expression evaluation    | Medium     | 1 `.c`      |

### Date and Time

| Utility   | Description                          | Complexity | Source Files |
|-----------|--------------------------------------|------------|-------------|
| `date`    | Display/set system date and time     | Medium     | 2 `.c`      |
| `sleep`   | Pause for specified duration         | Simple     | 1 `.c`      |

### System Information

| Utility          | Description                     | Complexity | Source Files |
|------------------|---------------------------------|------------|-------------|
| `df`             | Report filesystem space usage   | Complex    | 1 `.c`      |
| `hostname`       | Get/set system hostname         | Simple     | 1 `.c`      |
| `domainname`     | Get/set NIS domain name         | Simple     | 1 `.c`      |
| `nproc`          | Count available processors      | Simple     | 1 `.c`      |
| `freebsd-version`| Show FreeBSD version (compat)   | Simple     | Shell script|
| `uuidgen`        | Generate UUIDs                  | Simple     | 1 `.c`      |

### Shells

| Utility | Description                          | Complexity | Source Files |
|---------|--------------------------------------|------------|-------------|
| `sh`    | POSIX-compatible shell               | Very High  | 60+ `.c`    |
| `csh`   | C-shell (tcsh port)                  | Very High  | 30+ `.c`    |

### Archive and Mail

| Utility | Description                          | Complexity | Source Files |
|---------|--------------------------------------|------------|-------------|
| `pax`   | POSIX archive utility (tar/cpio)     | Complex    | 30+ `.c`    |
| `rmail` | Remote mail handler                  | Simple     | 1 `.c`      |

### Miscellaneous

| Utility     | Description                        | Complexity | Source Files |
|-------------|------------------------------------|------------|-------------|
| `sync`      | Flush filesystem buffers           | Simple     | 1 `.c`      |
| `stty`      | Set terminal characteristics       | Medium     | 2+ `.c`     |
| `cpuset`    | CPU affinity management            | Medium     | 1 `.c`      |

## Shared Components

The `contrib/` directory provides libraries shared across utilities:

### `contrib/libc-vis/`
BSD `vis(3)` and `unvis(3)` functions for encoding and decoding special
characters. Used by `ls` for safe filename display and by `pax` for
header encoding.

### `contrib/libedit/`
BSD `editline(3)` library providing command-line editing with history and
completion support. Used by `csh` and `sh` for interactive input.

### `contrib/printf/`
Shared `printf` format string processing used by multiple utilities that
need custom format string expansion beyond standard `printf(3)`.

## Project Structure

```
corebinutils/
├── configure           # Top-level configure script (POSIX sh)
├── README.md           # Build instructions
├── .gitattributes      # Git configuration
├── .gitignore          # Build artifact exclusions
├── contrib/            # Shared libraries
│   ├── libc-vis/       # vis(3)/unvis(3)
│   ├── libedit/        # editline(3)
│   └── printf/         # Shared printf helpers
├── cat/                # Each utility in its own directory
│   ├── cat.c           # Main source
│   ├── GNUmakefile     # Per-utility build rules
│   ├── cat.1           # Manual page
│   └── README.md       # Port-specific notes
├── chmod/
│   ├── chmod.c
│   ├── mode.c          # Shared mode parsing library
│   ├── mode.h
│   └── GNUmakefile
├── ...                 # (33 utility directories total)
└── sh/                 # Full POSIX shell (60+ source files)
```

## Utility Complexity Classification

### Tier 1 — Simple (1 source file, <500 lines)

`cat`, `echo`, `hostname`, `domainname`, `nproc`, `pwd`, `realpath`, `rmdir`,
`sleep`, `sync`, `uuidgen`, `pwait`

These utilities typically have a `main()` function that parses options with
`getopt(3)`, performs a single system call, and exits. Error handling follows
the `err(3)`/`warn(3)` pattern.

### Tier 2 — Medium (1-3 source files, 500-2000 lines)

`chmod` (with `mode.c`), `cp` (with `utils.c`, `fts.c`), `date` (with
`vary.c`), `kill`, `ln`, `mkdir` (with `mode.c`), `mv`, `rm`, `test`,
`timeout`, `expr`, `df`

These utilities involve more complex option parsing, recursive directory
traversal, or multi-step algorithms. They share code through header files
and sometimes reuse `mode.c`/`mode.h`.

### Tier 3 — Complex (5+ source files, 2000+ lines)

`dd` (8 source files), `ed` (10 source files), `ls` (5 source files),
`ps` (6 source files), `pax` (30+ source files)

These are substantial programs with their own internal architecture:
- `dd`: argument parser, conversion engine, signal handling, I/O position logic
- `ed`: command parser, buffer manager, regex engine, undo system
- `ls`: stat engine, sort/compare, print/format, ANSI color
- `ps`: /proc parser, format string engine, process filter, output formatter

### Tier 4 — Shells (30-60+ source files)

`sh` and `csh` are full POSIX-compatible shells with lexers, parsers, job
control, signal handling, built-in commands, and editline integration.

## Key Differences from GNU Coreutils

| Feature                | Corebinutils (BSD)          | GNU Coreutils              |
|------------------------|-----------------------------|----------------------------|
| License                | BSD-3-Clause / BSD-2-Clause | GPL-3.0                    |
| Default libc           | musl                        | glibc                      |
| `echo` behavior        | No `-e` flag (BSD compat)   | `-e` for escape sequences  |
| `test` parser          | Recursive descent           | Varies by implementation   |
| `ls` birth time        | `statx(2)` syscall          | `statx(2)` or fallback     |
| `dd` progress          | SIGINFO + `status=progress` | `status=progress`          |
| `sleep` units          | `s`, `m`, `h`, `d` suffixes | `s`, `m`, `h`, `d` (GNU ext)|
| Build system           | `./configure` + `GNUmakefile`| Autotools (autoconf/automake)|
| Error functions        | `err(3)`/`warn(3)` from libc| `error()` from gnulib      |
| FTS implementation     | In-tree custom `fts.c`      | gnulib FTS or `nftw(3)`    |

## Signal Handling Conventions

Most utilities follow a consistent signal handling pattern:

- **SIGINFO / SIGUSR1**: Progress reporting. `dd`, `chmod`, `sleep`, and
  others install a handler that sets a `volatile sig_atomic_t` flag, which
  the main loop checks to print status information.

- **SIGINT**: Graceful termination. Utilities performing recursive operations
  check for pending signals between iterations.

- **SIGHUP**: In `ed`, triggers an emergency save of the edit buffer to a
  temporary file.

Signal handlers are installed via `sigaction(2)` rather than the legacy
`signal(2)` function, ensuring reliable semantics across platforms.

## Error Handling Patterns

All utilities exit with standardized codes:

| Exit Code | Meaning                                  |
|-----------|------------------------------------------|
| 0         | Success                                  |
| 1         | General failure                          |
| 2         | Usage error (invalid arguments)          |
| 124       | Command timed out (`timeout` only)       |
| 125       | `timeout` internal error                 |
| 126       | Command found but not executable         |
| 127       | Command not found                        |

Error messages follow the BSD pattern:
```c
error_errno("open %s", path);   // "mv: open /foo: Permission denied"
error_msg("invalid mode: %s", arg);  // "chmod: invalid mode: xyz"
```

Many utilities provide custom `error_errno()` / `error_msg()` wrappers that
prepend the program name, format the message, and optionally append
`strerror(errno)`.

## Memory Management

Corebinutils utilities follow BSD memory conventions:

- **Dynamic allocation**: `malloc(3)` with explicit `NULL` checks, typically
  wrapped in `xmalloc()` that calls `err(1, "malloc")` on failure.
- **No fixed-size buffers** for user-controlled data (paths, format strings).
- **Adaptive buffer sizing**: `cat` and `cp` scale I/O buffers based on
  available physical memory via `sysconf(_SC_PHYS_PAGES)`.
- **Explicit cleanup**: `free()` is called in long-running loops to avoid
  accumulation, though single-pass utilities may rely on process exit.

### Buffer Strategy Example (from `cat.c` and `cp/utils.c`):

```c
#define PHYSPAGES_THRESHOLD (32*1024)
#define BUFSIZE_MAX         (2*1024*1024)
#define BUFSIZE_SMALL       (128*1024)

if (sysconf(_SC_PHYS_PAGES) > PHYSPAGES_THRESHOLD)
    bufsize = MIN(BUFSIZE_MAX, MAXPHYS * 8);
else
    bufsize = BUFSIZE_SMALL;
```

## Testing

Each utility directory may contain its own test suite, invoked through:

```sh
make -f GNUmakefile test
```

Or for a specific utility:

```sh
make -f GNUmakefile check-cat
make -f GNUmakefile check-ls
```

Tests that require root privileges or specific kernel features print `SKIP`
and continue without failing the overall test run.

## Building Quick Reference

```sh
cd corebinutils/
./configure                          # Detect toolchain, generate build files
make -f GNUmakefile -j$(nproc) all   # Build all utilities
make -f GNUmakefile test             # Run test suites
make -f GNUmakefile stage            # Copy binaries to out/bin/
make -f GNUmakefile install          # Install to $PREFIX/bin
```

See [building.md](building.md) for detailed configure options and build
customization.

## Further Reading

- [architecture.md](architecture.md) — Build system internals, code organization
- [building.md](building.md) — Configure options, dependencies, cross-compilation
- Individual utility documentation: [cat.md](cat.md), [ls.md](ls.md),
  [dd.md](dd.md), [ps.md](ps.md), etc.
- [code-style.md](code-style.md) — C coding conventions
- [error-handling.md](error-handling.md) — Error patterns and exit codes
