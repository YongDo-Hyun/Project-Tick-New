# Corebinutils — Architecture

## Repository Layout

The corebinutils tree follows a straightforward directory-per-utility layout
with a top-level orchestrator build system:

```
corebinutils/
├── configure               # POSIX sh configure script
├── README.md               # Top-level build instructions
├── .gitattributes
├── .gitignore
│
├── config.mk               # [generated] feature detection results
├── GNUmakefile              # [generated] top-level build orchestrator
│
├── build/                   # [generated] intermediate object files
│   ├── configure/           # Configure test artifacts and logs
│   ├── cat/                 # Per-utility build intermediates
│   ├── chmod/
│   ├── ...
│   └── sh/
│
├── out/                     # [generated] final binaries
│   └── bin/                 # Staged executables (after `make stage`)
│
├── contrib/                 # Shared library sources
│   ├── libc-vis/            # vis(3)/unvis(3) implementation
│   ├── libedit/             # editline(3) library
│   └── printf/              # Shared printf format helpers
│
├── cat/                     # Utility: cat
│   ├── cat.c                # Main source
│   ├── cat.1                # Manual page (groff)
│   ├── GNUmakefile          # Per-utility build rules
│   └── README.md            # Port notes and differences
│
├── chmod/                   # Utility: chmod
│   ├── chmod.c              # Main implementation
│   ├── mode.c               # Mode parsing library (shared with mkdir)
│   ├── mode.h               # Mode parsing header
│   ├── GNUmakefile
│   └── chmod.1
│
├── dd/                      # Utility: dd (multi-file)
│   ├── dd.c                 # Main control flow
│   ├── dd.h                 # Shared types (IO, STAT, flags)
│   ├── extern.h             # Function declarations
│   ├── args.c               # JCL argument parser
│   ├── conv.c               # Conversion functions (block/unblock/def)
│   ├── conv_tab.c           # ASCII/EBCDIC conversion tables
│   ├── gen.c                # Signal handling helpers
│   ├── misc.c               # Summary, progress, timing
│   ├── position.c           # Input/output seek positioning
│   └── GNUmakefile
│
├── ed/                      # Utility: ed (multi-file)
│   ├── main.c               # Command dispatch and main loop
│   ├── ed.h                 # Types (line_t, undo_t, constants)
│   ├── compat.c / compat.h  # Portability shims
│   ├── buf.c                # Buffer management (scratch file)
│   ├── glbl.c               # Global command (g/re/cmd)
│   ├── io.c                 # File I/O (read_file, write_file)
│   ├── re.c                 # Regular expression handling
│   ├── sub.c                # Substitution command
│   └── undo.c               # Undo stack management
│
├── ls/                      # Utility: ls (multi-file)
│   ├── ls.c                 # Main logic, option parsing, directory traversal
│   ├── ls.h                 # Types (entry, context, enums)
│   ├── extern.h             # Cross-module declarations
│   ├── print.c              # Output formatting (columns, long, stream)
│   ├── cmp.c                # Sort comparison functions
│   └── util.c               # Helper functions
│
├── ps/                      # Utility: ps (multi-file)
│   ├── ps.c                 # Main logic, /proc scanning
│   ├── ps.h                 # Types (kinfo_proc, KINFO, VAR)
│   ├── extern.h             # Cross-module declarations
│   ├── fmt.c                # Format string parsing
│   ├── keyword.c            # Output keyword definitions
│   ├── print.c              # Field value formatting
│   └── nlist.c              # Name list handling
│
└── sh/                      # Utility: POSIX shell
    ├── main.c               # Shell entry point
    ├── parser.c / parser.h  # Command parser
    ├── eval.c               # Command evaluator
    ├── exec.c               # Command execution
    ├── jobs.c               # Job control
    ├── var.c                # Variable management
    ├── trap.c               # Signal/trap handling
    ├── expand.c             # Parameter expansion
    ├── redir.c              # I/O redirection
    └── ...                  # (60+ additional files)
```

## Build System Architecture

### Two-Level Build Organization

The build system has two distinct levels:

1. **Top-level orchestrator** — Generated `GNUmakefile` and `config.mk` that
   coordinate all subdirectories.
2. **Per-utility `GNUmakefile`** — Each utility directory has its own build
   rules. These are the source of truth and are never overwritten by
   `configure`.

The top-level `GNUmakefile` invokes subdirectory builds via recursive make:

```makefile
build-%: prepare-%
	+env CPPFLAGS="$(CPPFLAGS)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
		$(MAKE) -C "$*" -f GNUmakefile $(SUBMAKE_OVERRIDES) all
```

### Shared Output Directories

All utilities share centralized output directories to simplify packaging:

```
build/    # Object files, organized per-utility: build/cat/, build/chmod/, ...
out/      # Final linked binaries
out/bin/  # Staged binaries (after `make stage`)
```

Subdirectories get symbolic links (`build -> ../build/<util>`,
`out -> ../out`) created by the `prepare-%` target:

```makefile
prepare-%:
	@mkdir -p "$(MONO_BUILDDIR)/$*" "$(MONO_OUTDIR)"
	@ln -sfn "../build/$*" "$*/build"
	@ln -sfn "../out" "$*/out"
```

### Variable Propagation

The top-level Makefile passes all detected toolchain variables to
subdirectory builds via `SUBMAKE_OVERRIDES`:

```makefile
SUBMAKE_OVERRIDES = \
	CC="$(CC)" \
	AR="$(AR)" \
	AWK="$(AWK)" \
	RANLIB="$(RANLIB)" \
	NM="$(NM)" \
	SH="$(SH)" \
	CRYPTO_LIBS="$(CRYPTO_LIBS)" \
	EDITLINE_CPPFLAGS="$(EDITLINE_CPPFLAGS)" \
	EDITLINE_LIBS="$(EDITLINE_LIBS)" \
	PREFIX="$(PREFIX)" \
	BINDIR="$(BINDIR)" \
	DESTDIR="$(DESTDIR)" \
	CROSS_COMPILING="$(CROSS_COMPILING)" \
	EXEEXT="$(EXEEXT)"
```

This ensures every utility builds with the same compiler, flags, and
library configuration.

### Generated vs. Maintained Files

| File             | Generated? | Purpose                              |
|------------------|------------|--------------------------------------|
| `configure`      | No         | POSIX sh configure script            |
| `config.mk`     | Yes        | Feature detection macros              |
| `GNUmakefile`    | Yes        | Top-level orchestrator                |
| `*/GNUmakefile`  | No         | Per-utility build rules               |
| `build/`         | Yes        | Object file directory tree            |
| `out/`           | Yes        | Binary output directory               |

## Configure Script Architecture

### Script Structure

The `configure` script is a single POSIX shell file (no autoconf) organized
into these phases:

```
1. Initialization       — Set defaults, parse CLI arguments
2. Compiler Detection   — Find musl-first C compiler
3. Tool Detection       — Find make, ar, ranlib, nm, awk, sh, pkg-config
4. Libc Identification  — Determine musl vs glibc via binary inspection
5. Header Probing       — Check for ~40 system headers
6. Function Probing     — Check for ~20 C library functions
7. Library Probing      — Check for optional libraries (crypt, dl, pthread, rt)
8. File Generation      — Write config.mk and GNUmakefile
```

### Compiler Probing

The compiler detection uses three progressive tests:

```sh
# Can it compile a simple program?
can_compile_with() { ... }

# Can it compile AND run? (native builds only)
can_run_with() { ... }

# Does it support C11 stdatomic.h?
can_compile_stdatomic_with() { ... }
```

All three must pass. For cross-compilation (`--host != --build`), the
run test is skipped.

### Feature Detection Pattern

Headers and functions are probed with a consistent pattern that records
results as Make variables and C preprocessor defines:

```sh
check_header() {
    hdr=$1
    macro="HAVE_$(to_macro "$hdr")"    # e.g., HAVE_SYS_ACL_H
    if try_cc "#include <$hdr>
    int main(void) { return 0; }"; then
        record_cpp_define "$macro" 1
    else
        record_cpp_define "$macro" 0
    fi
}

check_func() {
    func=$1
    includes=$2
    macro="HAVE_$(to_macro "$func")"   # e.g., HAVE_COPY_FILE_RANGE
    if try_cc "$includes
    int main(void) { void *p = (void *)(uintptr_t)&$func; return p == 0; }"; then
        record_cpp_define "$macro" 1
    else
        record_cpp_define "$macro" 0
    fi
}
```

### Headers Probed

The configure script checks for the following headers:

```
stdlib.h  stdio.h  stdint.h  inttypes.h  stdbool.h  stddef.h
string.h  strings.h  unistd.h  errno.h  fcntl.h  signal.h
sys/types.h  sys/stat.h  sys/time.h  sys/resource.h  sys/wait.h
sys/select.h  sys/ioctl.h  sys/param.h  sys/socket.h  netdb.h
poll.h  sys/poll.h  termios.h  stropts.h  pthread.h
sys/event.h  sys/timerfd.h  sys/acl.h  attr/xattr.h  linux/xattr.h
dlfcn.h  langinfo.h  locale.h  wchar.h  wctype.h
```

### Functions Probed

```
getcwd  realpath  fchdir  fstatat  openat  copy_file_range
memmove  strlcpy  strlcat  explicit_bzero  getline  getentropy
posix_spawn  clock_gettime  poll  kqueue  timerfd_create
pipe2  closefrom  getrandom
```

### Libraries Probed

| Library  | Symbol              | Usage                              |
|----------|---------------------|------------------------------------|
| crypt    | `crypt()`           | Password hashing (`ed -x` legacy)  |
| dl       | `dlopen()`          | Dynamic loading                    |
| pthread  | `pthread_create()`  | Threading support                  |
| rt       | `clock_gettime()`   | High-resolution timing             |
| util     | `openpty()`         | Pseudo-terminal support            |
| attr     | `setxattr()`        | Extended attributes (`mv`, `cp`)   |
| selinux  | `is_selinux_enabled()` | SELinux label support           |

## Code Organization Patterns

### Single-File Utility Pattern

Most simple utilities follow this structure:

```c
/* SPDX license header */

#include <system-headers.h>

struct options { ... };

static const char *progname;

static void usage(void) __attribute__((__noreturn__));
static void error_errno(const char *, ...);
static void error_msg(const char *, ...);

int main(int argc, char *argv[])
{
    struct options opt;
    int ch;

    progname = program_name(argv[0]);

    while ((ch = getopt(argc, argv, "...")) != -1) {
        switch (ch) {
        case 'f': opt.force = true; break;
        /* ... */
        default:  usage();
        }
    }
    argc -= optind;
    argv += optind;

    /* Perform main operation */
    for (int i = 0; i < argc; i++) {
        if (process(argv[i], &opt) != 0)
            exitval = 1;
    }
    return exitval;
}
```

### Multi-File Utility Pattern

Complex utilities split across files with a shared header:

```
utility/
├── utility.c     # main(), option parsing, top-level dispatch
├── utility.h     # Shared types, constants, macros
├── extern.h      # Function declarations for cross-module calls
├── sub1.c        # Functional subsystem (e.g., args.c, conv.c)
├── sub2.c        # Another subsystem (e.g., print.c, fmt.c)
└── GNUmakefile   # Build rules listing all .c files
```

### Header Guard Convention

Headers use the BSD `_FILENAME_H_` pattern:

```c
#ifndef _PS_H_
#define _PS_H_
/* ... */
#endif
```

### Portability Macros

Common compatibility macros appear across multiple utilities:

```c
#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

#ifndef __dead2
#define __dead2  __attribute__((__noreturn__))
#endif

#ifndef nitems
#define nitems(array) (sizeof(array) / sizeof((array)[0]))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
```

### POSIX Feature Test Macros

Many utilities define feature test macros at the top of their main source
file:

```c
#define _POSIX_C_SOURCE 200809L
```

Or rely on the configure-injected flags:

```
-D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700
```

## Shared Code Reuse

### `mode.c` / `mode.h`

The mode parsing library is shared between `chmod` and `mkdir`. It provides:

- `mode_compile()` — Parse a mode string (numeric or symbolic) into a
  compiled command array (`bitcmd_t`)
- `mode_apply()` — Apply a compiled mode to an existing `mode_t`
- `mode_free()` — Release compiled mode memory
- `strmode()` — Convert `mode_t` to display string like `"drwxr-xr-x "`

### `fts.c` / `fts.h`

An in-tree FTS (File Tree Walk) implementation used by `cp`, `chflags`, and
other utilities that do recursive directory traversal. This avoids depending
on glibc's FTS implementation or `nftw(3)`.

### `contrib/libc-vis/`

BSD `vis(3)` / `unvis(3)` character encoding used by `ls` for safe
display of filenames containing control characters or non-printable bytes.

### Signal Name Tables

`kill` and `timeout` both maintain identical `struct signal_entry` tables
mapping signal names to numbers:

```c
struct signal_entry {
    const char *name;
    int number;
};

#define SIGNAL_ENTRY(name) { #name, SIG##name }

static const struct signal_entry canonical_signals[] = {
    SIGNAL_ENTRY(HUP),
    SIGNAL_ENTRY(INT),
    SIGNAL_ENTRY(QUIT),
    /* ... ~30 standard signals ... */
};
```

Both also share the same `normalize_signal_name()` function pattern that
strips "SIG" prefixes and uppercases input.

## Data Structures

### Process Information (`ps`)

The `ps` utility defines a Linux-compatible replacement for FreeBSD's
`kinfo_proc`:

```c
struct kinfo_proc {
    pid_t    ki_pid, ki_ppid, ki_pgid, ki_sid;
    dev_t    ki_tdev;
    uid_t    ki_uid, ki_ruid, ki_svuid;
    gid_t    ki_groups[KI_NGROUPS];
    char     ki_comm[COMMLEN];        // 256 bytes
    struct timeval ki_start;
    uint64_t ki_runtime;              // microseconds
    uint64_t ki_size;                 // VSZ in bytes
    uint64_t ki_rssize;               // RSS in pages
    int      ki_nice;
    char     ki_stat;                 // BSD-like state (S,R,T,Z,D)
    int      ki_numthreads;
    struct rusage ki_rusage;
    /* ... */
};
```

This struct is populated by reading `/proc/[pid]/stat` and
`/proc/[pid]/status` files.

### I/O State (`dd`)

The `dd` utility uses two key structures for its I/O engine:

```c
typedef struct {
    u_char *db;         // Buffer address
    u_char *dbp;        // Current buffer I/O position
    ssize_t dbcnt;      // Current byte count in buffer
    ssize_t dbrcnt;     // Last read byte count
    ssize_t dbsz;       // Block size
    u_int   flags;      // ISCHR | ISPIPE | ISTAPE | ISSEEK | NOREAD | ISTRUNC
    const char *name;   // Filename
    int     fd;         // File descriptor
    off_t   offset;     // Block count to skip
    off_t   seek_offset;// Sparse output seek offset
} IO;

typedef struct {
    uintmax_t in_full, in_part;    // Full/partial input blocks
    uintmax_t out_full, out_part;  // Full/partial output blocks
    uintmax_t trunc;               // Truncated records
    uintmax_t swab;                // Odd-length swab blocks
    uintmax_t bytes;               // Total bytes written
    struct timespec start;         // Start timestamp
} STAT;
```

### Line Buffer (`ed`)

The `ed` editor uses a doubly-linked list of line nodes with a scratch
file backing store:

```c
typedef struct line {
    struct line *q_forw;   // Next line
    struct line *q_back;   // Previous line
    off_t        seek;     // Offset in scratch file
    int          len;      // Line length
} line_t;
```

### File Entry (`ls`)

The `ls` utility represents each directory entry with:

```c
struct entry {
    struct stat sb;
    struct file_time btime;    // Birth time (via statx)
    char *name;                // Display name
    char *link_target;         // Symlink target (if applicable)
    /* color, type classification, etc. */
};
```

## Makefile Targets Reference

### Top-Level Targets

| Target             | Description                                           |
|--------------------|-------------------------------------------------------|
| `all`              | Build all utilities                                   |
| `clean`            | Remove `build/` and `out/` directories                |
| `distclean`        | `clean` + remove generated `GNUmakefile`, `config.mk` |
| `rebuild`          | `clean` then `all`                                    |
| `reconfigure`      | Re-run `./configure`                                  |
| `check` / `test`   | Run all utility test suites                           |
| `stage`            | Copy binaries to `out/bin/`                           |
| `install`          | Copy binaries to `$DESTDIR$BINDIR`                    |
| `status`           | Show `out/` directory contents                        |
| `list`             | Print all subdirectory names                          |
| `print-config`     | Show active compiler and flags                        |
| `help`             | List available targets                                |

### Per-Utility Targets

Individual utilities can be built, cleaned, or tested:

```sh
make -f GNUmakefile build-cat      # Build only cat
make -f GNUmakefile clean-cat      # Clean only cat
make -f GNUmakefile check-cat      # Test only cat
make -f GNUmakefile cat            # Alias for build-cat
```

### Target Dependencies

```
all
 └── build-<util> (for each utility)
      └── prepare-<util>
           ├── mkdir -p build/<util> out/
           ├── ln -sfn ../build/<util> <util>/build
           └── ln -sfn ../out <util>/out

stage
 └── all
      └── copy executables to out/bin/

install
 └── stage
      └── copy out/bin/* to $DESTDIR$BINDIR/

distclean
 └── clean
      └── remove build/ out/
 └── unprepare
      └── remove build/out symlinks from subdirs
 └── remove GNUmakefile config.mk
```

## Cross-Compilation Support

The configure script supports cross-compilation via `--host` and `--build`
triples:

```sh
./configure --host=aarch64-linux-musl --build=x86_64-linux-musl \
            --cc=aarch64-linux-musl-gcc
```

When `--host` differs from `--build`:
- The executable run test (`can_run_with`) is skipped
- `CROSS_COMPILING=1` is recorded in `config.mk`
- The value propagates to all subdirectory builds

## Typical Per-Utility GNUmakefile

Each utility has a `GNUmakefile` following this general pattern:

```makefile
# cat/GNUmakefile

PROG = cat
SRCS = cat.c

BUILDDIR ?= build
OUTDIR ?= out

CC ?= cc
CPPFLAGS += -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700
CFLAGS ?= -O2 -g -pipe
LDFLAGS ?=

OBJS = $(SRCS:.c=.o)
OBJS := $(addprefix $(BUILDDIR)/,$(OBJS))

all: $(OUTDIR)/$(PROG)

$(OUTDIR)/$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(OUTDIR)/$(PROG)

test:
	@echo "SKIP: no tests for $(PROG)"

.PHONY: all clean test
```

Multi-file utilities list all sources in `SRCS` and may link additional
libraries:

```makefile
# dd/GNUmakefile
SRCS = dd.c args.c conv.c conv_tab.c gen.c misc.c position.c
LDLIBS += -lm    # For dd's speed calculations
```

## Security Considerations

### Input Validation Boundaries

- **File paths**: Validated against `PATH_MAX` limits. Utilities like `rm`
  explicitly reject `/`, `.`, and `..` as arguments.
- **Numeric arguments**: Parsed with `strtoimax()` or `strtol()` with
  explicit overflow checking.
- **Signal numbers**: Validated against the compiled signal table, not
  unchecked `atoi()`.
- **Mode strings**: `mode_compile()` validates syntax before any filesystem
  modification occurs.

### Privilege Handling

- `hostname` and `domainname` require root for set operations; they validate
  the hostname length against the kernel's UTS namespace limit first.
- `rm` refuses to delete `/` unless explicitly overridden.
- `chmod -R` includes cycle detection to prevent infinite loops from symlink
  chains.

### Temporary File Safety

- `ed` creates temporary scratch files in `$TMPDIR` (or `/tmp`) using
  `mkstemp(3)`.
- `dd` does not create temporary files — it operates on explicit input/output
  file descriptors.
