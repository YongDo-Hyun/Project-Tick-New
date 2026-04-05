# genqrcode / libqrencode — Building

## Overview

genqrcode (libqrencode 4.1.1) supports two build systems:

1. **CMake** — The recommended modern approach
2. **GNU Autotools** — The traditional `./configure && make` workflow

Both produce the same outputs: a static and/or shared library (`libqrencode`), the optional command-line tool (`qrencode`), and associated files (pkg-config, man page).

---

## Dependencies

### Required

None. The core library has **zero external dependencies**.

### Optional

| Dependency | Purpose | Detection Method |
|---|---|---|
| **pthreads** | Thread-safe RS encoding | CMake: `find_package(Threads)` / Autotools: `AC_CHECK_LIB([pthread])` |
| **libpng** | PNG output for CLI tool | CMake: `find_package(PNG)` / Autotools: `PKG_CHECK_MODULES(png, "libpng")` |
| **libiconv** | Test suite character conversion decoder | CMake: `find_package(Iconv)` / Autotools: `AM_ICONV_LINK` |
| **SDL 2.0** | `view_qrcode` test viewer | Autotools only: `PKG_CHECK_MODULES(SDL, [sdl2 >= 2.0.0])` |

### Build Tools Required

For **CMake**:
- CMake ≥ 3.1.0
- A C compiler (GCC, Clang, MSVC)

For **Autotools** (when building from a Git checkout):
- autoconf
- automake
- autotools-dev
- libtool
- pkg-config
- libpng-dev (for CLI tool)

> **Note:** If you downloaded a release tarball that already includes the `configure` script, you do not need autoconf/automake/libtool.

---

## CMake Build

### Source Files Compiled

The CMakeLists.txt defines the library sources explicitly:

```cmake
set(QRENCODE_SRCS qrencode.c
                  qrinput.c
                  bitstream.c
                  qrspec.c
                  rsecc.c
                  split.c
                  mask.c
                  mqrspec.c
                  mmask.c)

set(QRENCODE_HDRS qrencode_inner.h
                  qrinput.h
                  bitstream.h
                  qrspec.h
                  rsecc.h
                  split.h
                  mask.h
                  mqrspec.h
                  mmask.h)
```

### Configuration Options

| Option | Default | Description |
|---|---|---|
| `WITH_TOOLS` | `YES` | Build the `qrencode` CLI tool |
| `WITH_TESTS` | `NO` | Build test programs |
| `WITHOUT_PNG` | `NO` | Disable PNG support (even if libpng is found) |
| `BUILD_SHARED_LIBS` | `NO` | Build shared library instead of static |
| `GPROF` | `OFF` | Add `-pg` for gprof profiling |
| `COVERAGE` | `OFF` | Add `--coverage` for gcov |
| `ASAN` | `OFF` | Enable AddressSanitizer |

### Basic Build

```bash
cd genqrcode
mkdir build && cd build
cmake ..
make
```

### Build with All Options

```bash
cmake .. \
  -DWITH_TOOLS=YES \
  -DWITH_TESTS=YES \
  -DBUILD_SHARED_LIBS=YES \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Shared Library Build

```bash
cmake .. -DBUILD_SHARED_LIBS=YES
make
```

On MSVC, this automatically sets `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON`. The shared library gets proper versioning:

```cmake
set_target_properties(qrencode PROPERTIES
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    SOVERSION ${PROJECT_VERSION_MAJOR})
```

This produces `libqrencode.so.4.1.1` with symlinks `libqrencode.so.4` and `libqrencode.so`.

### Building Without PNG

If you don't need the CLI tool to output PNG files:

```bash
cmake .. -DWITHOUT_PNG=YES
```

The CLI tool will still be built (if `WITH_TOOLS=YES`) but will print an error if PNG output is requested at runtime.

### Building Tests

```bash
cmake .. -DWITH_TESTS=YES
make
ctest
```

When `WITH_TESTS=YES`:
- The `STATIC_IN_RELEASE` macro is defined as empty (not `static`), exposing internal functions
- The `WITH_TESTS` macro is defined, enabling test-only code paths
- The `tests/` subdirectory is included

The test CMakeLists.txt creates these test executables:

| Test | Required Dependencies |
|---|---|
| `test_bitstream` | None |
| `test_estimatebit` | None |
| `test_split` | None |
| `test_qrinput` | iconv |
| `test_qrspec` | iconv |
| `test_mqrspec` | iconv |
| `test_qrencode` | iconv |
| `test_split_urls` | iconv |
| `test_monkey` | iconv |
| `test_mask` | iconv + VLA support |
| `test_mmask` | iconv + VLA support |
| `test_rs` | iconv + VLA support |

### Sanitizer and Profiling Builds

```bash
# AddressSanitizer
cmake .. -DASAN=ON
make

# gprof profiling
cmake .. -DGPROF=ON
make

# Code coverage
cmake .. -DCOVERAGE=ON
make
# ... run tests ...
gcov *.c
```

The ASAN option adds `-fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls`.

### Cross-Compilation (MinGW)

The project includes `toolchain-mingw32.cmake` for cross-compiling to Windows:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw32.cmake
make
```

### Installation

```bash
sudo make install
```

Installed files:

| File | Destination |
|---|---|
| `libqrencode.a` / `.so` | `${CMAKE_INSTALL_LIBDIR}` |
| `qrencode.h` | `${CMAKE_INSTALL_INCLUDEDIR}` |
| `qrencode` (CLI) | `${CMAKE_INSTALL_BINDIR}` |
| `libqrencode.pc` | `${CMAKE_INSTALL_LIBDIR}/pkgconfig` |
| `qrencode.1` | `${CMAKE_INSTALL_MANDIR}/man1` |

The pkg-config file is generated from `libqrencode.pc.in`:

```
prefix=@prefix@
exec_prefix=@exec_prefix@
libdir=@libdir@
includedir=@includedir@

Name: libqrencode
Description: A QR Code encoding library
Version: @VERSION@
Libs: -L${libdir} -lqrencode @LIBPTHREAD@
Cflags: -I${includedir}
```

### CMake System Checks

The CMakeLists.txt performs these checks at configure time:

```cmake
check_include_file(dlfcn.h    HAVE_DLFCN_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(memory.h   HAVE_MEMORY_H)
check_include_file(stdint.h   HAVE_STDINT_H)
check_include_file(stdlib.h   HAVE_STDLIB_H)
check_include_file(strings.h  HAVE_STRINGS_H)
check_include_file(string.h   HAVE_STRING_H)
check_include_file(getopt.h   HAVE_GETOPT_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(time.h     HAVE_TIME_H)
check_include_file(pthread.h  HAVE_PTHREAD_H)

check_function_exists(strdup HAVE_STRDUP)
```

Preprocessor defines always set:
```cmake
add_definitions(-DMAJOR_VERSION=${PROJECT_VERSION_MAJOR})
add_definitions(-DMINOR_VERSION=${PROJECT_VERSION_MINOR})
add_definitions(-DMICRO_VERSION=${PROJECT_VERSION_PATCH})
add_definitions(-DVERSION="${PROJECT_VERSION}")
add_definitions(-DHAVE_SDL=0)
```

### MSVC-Specific Handling

On MSVC, additional definitions are added:

```cmake
add_definitions(-Dstrcasecmp=_stricmp)
add_definitions(-Dstrncasecmp=_strnicmp)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
```

When building the CLI tool on MSVC, `getopt.h` and `getopt.lib` must be found separately, as MSVC does not provide them:

```cmake
find_path(GETOPT_INCLUDE_DIR getopt.h PATH_SUFFIXES include)
find_library(GETOPT_LIBRARIES getopt PATH_SUFFIXES lib)
```

---

## Autotools Build

### Generating Configure Script

If building from Git (no `configure` script present):

```bash
./autogen.sh
```

This runs `autoreconf -i` to generate `configure`, `Makefile.in`, and related files. Required packages on Ubuntu/Debian:

```bash
sudo apt install autoconf automake autotools-dev libtool pkg-config
```

### Configuration

```bash
./configure [OPTIONS]
```

#### Configure Options

| Option | Default | Description |
|---|---|---|
| `--enable-thread-safety` | `yes` | Enable pthread-based thread safety |
| `--without-png` | (with png) | Disable libpng support |
| `--with-tools` | `yes` | Build CLI tool |
| `--without-tools` | — | Skip CLI tool |
| `--with-tests` | `no` | Build test suite |
| `--enable-gprof` | `no` | Enable gprof profiling (`-g -pg`) |
| `--enable-gcov` | `no` | Enable gcov coverage (`--coverage`) |
| `--enable-asan` | `no` | Enable AddressSanitizer |
| `--prefix=DIR` | `/usr/local` | Installation prefix |

### Basic Build

```bash
./configure
make
sudo make install
sudo ldconfig
```

### Disabling Tools

```bash
./configure --without-tools
make
```

### Building Tests

```bash
./configure --with-tests
make
make check
```

When `--with-tests` is given, `configure.ac` also:
- Checks for SDL 2.0 (for the `view_qrcode` viewer)
- Checks for iconv (for the test decoder)
- Defines `STATIC_IN_RELEASE` as empty
- Defines `WITH_TESTS=1`

### Thread Safety

Thread safety is enabled by default:

```bash
# Explicitly enable (default)
./configure --enable-thread-safety

# Disable
./configure --disable-thread-safety
```

When enabled, `configure.ac` checks for `pthread_mutex_init` in libpthread. If found:
- `HAVE_LIBPTHREAD=1` is defined
- `-pthread` is added to `CFLAGS`
- `-lpthread` is added to the linker flags (via `LIBPTHREAD` substitution)

### Library Versioning (Autotools)

The Automake configuration uses libtool versioning:

```makefile
libqrencode_la_LDFLAGS = -version-number $(MAJOR_VERSION):$(MINOR_VERSION):$(MICRO_VERSION)
```

For version 4.1.1, this produces `libqrencode.so.4.1.1`.

### MinGW Cross-Compilation

The configure script detects MinGW targets:

```m4
case "${target}" in
*-*-mingw*)
    mingw=yes
esac
```

On MinGW, additional linker flags are applied:

```makefile
libqrencode_la_LDFLAGS += -no-undefined -avoid-version -Wl,--nxcompat -Wl,--dynamicbase
```

### Full Configure Example

```bash
./configure \
  --prefix=/opt/qrencode \
  --enable-thread-safety \
  --with-tools \
  --with-tests \
  --enable-asan
make -j$(nproc)
make check
sudo make install
```

### Source Distribution

To create a source tarball:

```bash
make dist
```

The `EXTRA_DIST` variable ensures additional files are included:

```makefile
EXTRA_DIST = libqrencode.pc.in autogen.sh configure.ac acinclude.m4 \
             Makefile.am tests/Makefile.am \
             qrencode.1.in Doxyfile \
             CMakeLists.txt cmake/FindIconv.cmake
```

---

## vcpkg

libqrencode is available through Microsoft's vcpkg package manager:

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg integrate install
./vcpkg install libqrencode
```

---

## Using the Installed Library

### pkg-config

```bash
# Compile
gcc -c myapp.c $(pkg-config --cflags libqrencode)

# Link
gcc -o myapp myapp.o $(pkg-config --libs libqrencode)
```

### CMake (as dependency)

In your application's `CMakeLists.txt`:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(QRENCODE REQUIRED libqrencode)

target_include_directories(myapp PRIVATE ${QRENCODE_INCLUDE_DIRS})
target_link_libraries(myapp ${QRENCODE_LIBRARIES})
```

Or directly:

```cmake
find_library(QRENCODE_LIB qrencode)
find_path(QRENCODE_INCLUDE qrencode.h)

target_include_directories(myapp PRIVATE ${QRENCODE_INCLUDE})
target_link_libraries(myapp ${QRENCODE_LIB})
```

### Direct Compilation

For simple projects, you can compile the library sources directly into your project:

```bash
gcc -c qrencode.c qrinput.c bitstream.c qrspec.c rsecc.c split.c mask.c mqrspec.c mmask.c
ar rcs libqrencode.a qrencode.o qrinput.o bitstream.o qrspec.o rsecc.o split.o mask.o mqrspec.o mmask.o
gcc -o myapp myapp.c -L. -lqrencode
```

You must define the version macros:
```bash
gcc -DMAJOR_VERSION=4 -DMINOR_VERSION=1 -DMICRO_VERSION=1 \
    -DVERSION=\"4.1.1\" -DHAVE_STRDUP=1 \
    -DSTATIC_IN_RELEASE=static \
    -c qrencode.c qrinput.c bitstream.c qrspec.c rsecc.c split.c mask.c mqrspec.c mmask.c
```

---

## Build Output Summary

| Build Target | File | Description |
|---|---|---|
| Library (static) | `libqrencode.a` | Static library |
| Library (shared) | `libqrencode.so.4.1.1` | Shared library |
| CLI tool | `qrencode` | Command-line encoder (built from `qrenc.c`) |
| pkg-config | `libqrencode.pc` | Generated from `libqrencode.pc.in` |
| Man page | `qrencode.1` | Generated from `qrencode.1.in` |
| Public header | `qrencode.h` | Only public header installed |

---

## Configuration Summary Output

The CMake build prints a detailed configuration summary:

```
------------------------------------------------------------
[QRencode] Configuration summary.
------------------------------------------------------------
 System configuration:
 .. Processor type .............. = x86_64
 .. CMake version ............... = 3.x.x
 Dependencies:
 .. Thread library .............. = -lpthread
 .. Iconv ....................... = TRUE
 .. PNG ......................... = TRUE
 Project configuration:
 .. Build test programs ......... = YES
 .. Build utility tools ......... = YES
 .. Disable PNG support ......... = NO
 .. Installation prefix ......... = /usr/local
------------------------------------------------------------
```

The Autotools build prints compiler flags:

```
Options used to compile and link:
  CC       = gcc
  CFLAGS   = -Wall -pthread
  CXX      = g++
  LDFLAGS  =
```

---

## Generating API Documentation

The project includes a `Doxyfile` for generating API documentation with Doxygen:

```bash
doxygen Doxyfile
```

This generates HTML documentation from the `qrencode.h` header comments. The documentation is also available online at:
https://fukuchi.org/works/qrencode/manual/index.html

---

## Troubleshooting

### "configure: error: cannot find install-sh, install.sh, or shtool"

Run `./autogen.sh` first to generate the Autotools infrastructure.

### "PNG output is disabled at compile time"

The CLI tool was built without libpng. Install libpng-dev and rebuild, or use `-DWITHOUT_PNG=NO` with CMake.

### Test failures with missing iconv

Many tests require iconv for the decoder library. Install libiconv or your system's iconv implementation. On Ubuntu:

```bash
sudo apt install libc6-dev  # glibc includes iconv
```

### "undefined reference to `pthread_mutex_init`"

Add `-lpthread` to your linker flags, or rebuild with `--disable-thread-safety` if you don't need thread safety.

### CMake can't find getopt.h (MSVC)

On Windows with MSVC, getopt.h is not available by default. Install a compatible getopt implementation, or disable tool building with `-DWITH_TOOLS=NO`.
