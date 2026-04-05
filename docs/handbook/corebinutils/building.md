# Corebinutils — Building

## Prerequisites

### Required Tools

| Tool       | Minimum Version | Purpose                                 |
|------------|----------------|-----------------------------------------|
| C compiler | C11 support    | Must support `<stdatomic.h>`            |
| `make`     | GNU Make 4.x   | Build orchestration                     |
| `ar`       | Any            | Archive tool for static libraries       |
| `ranlib`   | Any            | Library index generation                |
| `awk`      | POSIX          | Build-time text processing              |
| `sh`       | POSIX          | Shell for scripts and tests             |

### Preferred Compiler: musl-based

The configure script searches for musl-based compilers in this priority
order:

1. `musl-clang` — musl's Clang wrapper
2. `clang --target=<arch>-linux-musl` — Clang targeting musl
3. `clang --target=<arch>-unknown-linux-musl` — Clang with full triple
4. `musl-gcc` — musl's GCC wrapper
5. `clang` — Generic Clang (libc detected from output binary)
6. `cc` — System default
7. `gcc` — GNU CC

If a glibc toolchain is detected, configure fails with:

```
configure: error: glibc toolchain detected; refusing by default
                  (use --allow-glibc to override)
```

### Libc Detection

The configure script identifies the libc implementation through three
methods (tried in order):

1. **Binary inspection**: Compiles a test program, runs `file(1)` on it,
   looks for `ld-musl` or `ld-linux` in the interpreter path
2. **Preprocessor macros**: Checks for `__GLIBC__` or `__MUSL__` in the
   compiler's predefined macros
3. **Target triple**: Inspects the compiler's `-dumpmachine` output for
   `musl` or `gnu`/`glibc` substrings

### Optional Dependencies

| Library    | Symbol           | Required By         | Fallback             |
|------------|------------------|---------------------|----------------------|
| `libcrypt` | `crypt()`        | `ed` (legacy `-x`)  | Feature disabled     |
| `libdl`    | `dlopen()`       | `sh` (loadable)     | Feature disabled     |
| `libpthread`| `pthread_create()` | Various           | Single-threaded      |
| `librt`    | `clock_gettime()`| `dd`, `timeout`     | Linked if needed     |
| `libutil`  | `openpty()`      | `sh`, `csh`         | Pty feature disabled |
| `libattr`  | `setxattr()`     | `mv`, `cp`          | xattr not preserved  |
| `libselinux`| `is_selinux_enabled()` | SELinux labels | Labels not set     |
| `libedit`  | editline(3)      | `sh`, `csh`         | No line editing      |

## Quick Build

```sh
cd corebinutils/

# Step 1: Configure
./configure

# Step 2: Build all utilities
make -f GNUmakefile -j$(nproc) all

# Step 3: (Optional) Run tests
make -f GNUmakefile test

# Step 4: (Optional) Stage binaries
make -f GNUmakefile stage
```

After a successful build, binaries appear in `out/` and staged copies in
`out/bin/`.

## Configure Script Reference

### Usage

```
./configure [options]
```

### General Options

| Option                    | Default          | Description                    |
|---------------------------|------------------|--------------------------------|
| `--help`                  |                  | Show help and exit             |
| `--prefix=PATH`           | `/usr/local`     | Install prefix                 |
| `--bindir=PATH`           | `<prefix>/bin`   | Install binary directory       |
| `--host=TRIPLE`           | Auto-detected    | Target host triple             |
| `--build=TRIPLE`          | Auto-detected    | Build system triple            |

### Toolchain Options

| Option                    | Default          | Description                    |
|---------------------------|------------------|--------------------------------|
| `--cc=COMMAND`            | Auto-detected    | Force specific compiler        |
| `--allow-glibc`           | Off              | Allow glibc toolchain          |

### Flag Options

| Option                    | Default          | Description                    |
|---------------------------|------------------|--------------------------------|
| `--extra-cppflags=FLAGS`  | Empty            | Extra preprocessor flags       |
| `--extra-cflags=FLAGS`    | Empty            | Extra compilation flags        |
| `--extra-ldflags=FLAGS`   | Empty            | Extra linker flags             |

### Local Path Options

| Option                    | Default          | Description                    |
|---------------------------|------------------|--------------------------------|
| `--with-local-dir=PATH`   | `/usr/local`     | Add `PATH/include` and `PATH/lib` |
| `--without-local-dir`     |                  | Disable local path probing     |

### Policy Options

| Option                       | Default | Description                        |
|------------------------------|---------|-----------------------------------|
| `--enable-fail-if-missing`   | Off     | Fail on missing optional probes   |

### Environment Variables

The configure script respects standard environment variables:

| Variable    | Purpose                                        |
|-------------|------------------------------------------------|
| `CC`        | C compiler (overridden by probing if unusable) |
| `CPPFLAGS`  | Preprocessor flags from environment            |
| `CFLAGS`    | Compilation flags from environment             |
| `LDFLAGS`   | Linker flags from environment                  |

### Default Flags

The configure script applies these base flags:

```
CPPFLAGS: -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 $env $extra
CFLAGS:   -O2 -g -pipe $env $extra
LDFLAGS:  $env $extra
```

## Configure Output

### `config.mk`

A Make include file with feature detection results:

```makefile
# Auto-generated by ./configure on 2026-04-05T12:00:00Z
CONF_CPPFLAGS :=
CONF_LDFLAGS :=
CONF_LIBS :=
CONFIGURE_TIMESTAMP := 2026-04-05T12:00:00Z
CONFIGURE_HOST := x86_64-unknown-Linux
CONFIGURE_BUILD := x86_64-unknown-Linux
CONFIGURE_CC_MACHINE := x86_64-linux-musl
CONFIGURE_LIBC := musl
CROSS_COMPILING := 0
EXEEXT :=
HAVE_STDLIB_H := 1
HAVE_COPY_FILE_RANGE := 1
HAVE_STRLCPY := 1
# ... (one entry per probed header/function)
CONF_CPPFLAGS += -DHAVE_STDLIB_H=1 -DHAVE_COPY_FILE_RANGE=1 ...
CONF_LIBS += -lcrypt -ldl -lpthread -lrt
```

### `GNUmakefile`

The generated top-level Makefile contains:

- Toolchain variables (`CC`, `AR`, `RANLIB`, etc.)
- Flag variables (`CPPFLAGS`, `CFLAGS`, `LDFLAGS`)
- Library variables (`CRYPTO_LIBS`, `EDITLINE_LIBS`)
- Path variables (`PREFIX`, `BINDIR`, `MONO_BUILDDIR`, `MONO_OUTDIR`)
- Subdirectory list (`SUBDIRS := cat chflags chmod cp ...`)
- Build, clean, test, install, and utility targets

### `build/configure/config.log`

Detailed log of every compiler test and probe result. Useful for debugging
configure failures:

```
$ musl-clang -x c build/configure/conftest.c -o build/configure/conftest
$ build/configure/conftest
checking for sys/acl.h... no
checking for copy_file_range... yes
```

## Makefile Targets

### Build Targets

```sh
make -f GNUmakefile all             # Build all utilities
make -f GNUmakefile cat             # Build only cat (alias for build-cat)
make -f GNUmakefile build-cat       # Build only cat
make -f GNUmakefile build-ls        # Build only ls
```

### Clean Targets

```sh
make -f GNUmakefile clean           # Remove build/ and out/
make -f GNUmakefile clean-cat       # Clean only cat's objects
make -f GNUmakefile distclean       # clean + remove GNUmakefile, config.mk
make -f GNUmakefile maintainer-clean # Same as distclean
```

### Test Targets

```sh
make -f GNUmakefile test            # Run all test suites
make -f GNUmakefile check           # Same as test
make -f GNUmakefile check-cat       # Test only cat
make -f GNUmakefile check-ls        # Test only ls
```

### Install Targets

```sh
make -f GNUmakefile stage           # Copy binaries to out/bin/
make -f GNUmakefile install         # Install to $DESTDIR$PREFIX/bin
make -f GNUmakefile install DESTDIR=/tmp/pkg  # Staged install
```

### Information Targets

```sh
make -f GNUmakefile status          # Show output directory contents
make -f GNUmakefile list            # List all utility subdirectories
make -f GNUmakefile print-config    # Show compiler and flags
make -f GNUmakefile print-subdirs   # List subdirectories
make -f GNUmakefile help            # Show available targets
```

### Rebuild and Reconfigure

```sh
make -f GNUmakefile rebuild         # clean + all
make -f GNUmakefile reconfigure     # Re-run ./configure
```

## Cross-Compilation

### Basic Cross-Compilation

```sh
./configure \
    --host=aarch64-linux-musl \
    --build=x86_64-linux-musl \
    --cc=aarch64-linux-musl-gcc

make -f GNUmakefile -j$(nproc) all
```

### Cross-Compilation with Clang

```sh
./configure \
    --host=aarch64-linux-musl \
    --cc="clang --target=aarch64-linux-musl --sysroot=/path/to/musl-sysroot"

make -f GNUmakefile -j$(nproc) all
```

### Cross vs. Native Detection

When `--host` matches `--build` (or both are auto-detected to the same
value), `REQUIRE_RUNNABLE_CC=1` and the configure script verifies the
compiler produces executables that can actually run. For cross-compilation,
only compilation (not execution) is tested.

## Build Customization

### Custom Compiler Flags

```sh
# Debug build
./configure --extra-cflags="-O0 -g3 -fsanitize=address,undefined"

# Release build
./configure --extra-cflags="-O3 -DNDEBUG -flto" --extra-ldflags="-flto"

# With warnings
./configure --extra-cflags="-Wall -Wextra -Werror"
```

### Custom Install Prefix

```sh
./configure --prefix=/opt/project-tick --bindir=/opt/project-tick/sbin
make -f GNUmakefile -j$(nproc) all
make -f GNUmakefile install
```

### Building Individual Utilities

```sh
# Configure once
./configure

# Build only what you need
make -f GNUmakefile cat ls cp mv rm mkdir
```

### Forcing glibc

```sh
./configure --allow-glibc --cc=gcc
```

Note: The primary test target for corebinutils is musl. Building with glibc
may expose minor differences in header availability or function signatures.

## Troubleshooting

### "no usable compiler found"

The configure script could not find any C compiler that:
1. Produces working executables
2. Supports `<stdatomic.h>` (C11)
3. Can run the output (native builds only)

**Fix**: Install `musl-gcc` or `musl-clang`, or specify a compiler
explicitly with `--cc=...`.

### "glibc toolchain detected; refusing by default"

The detected compiler links against glibc instead of musl.

**Fix**: Install musl development tools or pass `--allow-glibc`.

### Missing header warnings

The configure log (`build/configure/config.log`) shows which headers were
not found. Missing optional headers (e.g., `sys/acl.h`) disable related
features but don't prevent building.

### Linker errors for `-lcrypt` or `-lrt`

Some utilities use optional libraries. If they're not found at configure
time, the corresponding features are disabled. If you see linker errors:

```sh
# Check what was detected
cat config.mk | grep CONF_LIBS
```

### Parallel build failures

If `make -j$(nproc)` fails but `make -j1` succeeds, a subdirectory
`GNUmakefile` may have missing dependencies. File a bug report.

### Cleaning stale state

```sh
make -f GNUmakefile distclean
./configure
make -f GNUmakefile -j$(nproc) all
```

## Build Output Structure

After a successful `make all && make stage`:

```
out/
├── cat
├── chmod
├── cp
├── csh
├── date
├── dd
├── df
├── echo
├── ed
├── expr
├── hostname
├── kill
├── ln
├── ls
├── mkdir
├── mv
├── nproc
├── pax
├── ps
├── pwd
├── realpath
├── rm
├── rmdir
├── sh
├── sleep
├── sync
├── test
├── timeout
├── [              # Symlink or hardlink to test
└── bin/           # Staged binaries (copies)
```

## CI Integration

For CI pipelines, use the non-interactive build sequence:

```sh
#!/bin/sh
set -eu

cd corebinutils/
./configure --prefix=/usr

# Build with parallelism hinted by configure
JOBS=$(grep -o 'JOBS_HINT := [0-9]*' GNUmakefile | cut -d' ' -f3)
make -f GNUmakefile -j"${JOBS:-1}" all

# Run tests (SKIP is OK, failures are not)
make -f GNUmakefile test

# Package
make -f GNUmakefile install DESTDIR="$PWD/pkg"
```
