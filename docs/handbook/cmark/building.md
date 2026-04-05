# cmark — Building

## Build System Overview

cmark uses CMake (minimum version 3.14) as its build system. The top-level `CMakeLists.txt` defines the project as C/CXX with version 0.31.2. It configures C99 standard without extensions, sets up export header generation, CTest integration, and subdirectory targets for the library, CLI tool, tests, man pages, and fuzz harness.

## Prerequisites

- A C99-compliant compiler (GCC, Clang, MSVC)
- CMake 3.14 or later
- POSIX environment (for man page generation; skipped on Windows)
- Optional: re2c (only needed if modifying `scanners.re`)
- Optional: Python 3 (for running spec tests)

## Basic Build Steps

```bash
# Out-of-source build (required — in-source builds are explicitly blocked)
mkdir build && cd build
cmake ..
make
```

The CMakeLists.txt enforces out-of-source builds with:

```cmake
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "Do not build in-source.\nPlease remove CMakeCache.txt and the CMakeFiles/ directory.\nThen: mkdir build ; cd build ; cmake .. ; make")
endif()
```

## CMake Configuration Options

### Library Type

```cmake
option(BUILD_SHARED_LIBS "Build the CMark library as shared" OFF)
```

By default, cmark builds as a **static library**. Set `-DBUILD_SHARED_LIBS=ON` for a shared library. When building as static, the compile definition `CMARK_STATIC_DEFINE` is automatically set.

**Legacy options** (deprecated but still functional for backwards compatibility):
- `CMARK_SHARED` — replaced by `BUILD_SHARED_LIBS`
- `CMARK_STATIC` — replaced by `BUILD_SHARED_LIBS` (inverted logic)

Both emit `AUTHOR_WARNING` messages advising migration to the standard CMake variable.

### Fuzzing Support

```cmake
option(CMARK_LIB_FUZZER "Build libFuzzer fuzzing harness" OFF)
```

When enabled, targets matching `fuzz` get `-fsanitize=fuzzer`, while all other targets get `-fsanitize=fuzzer-no-link`.

### Build Types

The project supports these build types via `CMAKE_BUILD_TYPE`:

| Type | Description |
|------|-------------|
| `Release` | Default. Optimized build |
| `Debug` | Adds `-DCMARK_DEBUG_NODES` for node integrity checking via `assert()` |
| `Profile` | Adds `-pg` for profiling with gprof |
| `Asan` | Address sanitizer (loads `FindAsan` module) |
| `Ubsan` | Adds `-fsanitize=undefined` for undefined behavior sanitizer |

Debug builds automatically add node structure checking:

```cmake
add_compile_options($<$<CONFIG:Debug>:-DCMARK_DEBUG_NODES>)
```

## Compiler Flags

The `cmark_add_compile_options()` function applies compiler warnings per-target (not globally), so cmark can be used as a subdirectory in projects with other languages:

**GCC/Clang:**
```
-Wall -Wextra -pedantic -Wstrict-prototypes  (C only)
```

**MSVC:**
```
-D_CRT_SECURE_NO_WARNINGS
```

Visibility is set globally to hidden, with explicit export via the generated `cmark_export.h`:

```cmake
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
```

## Library Target: `cmark`

Defined in `src/CMakeLists.txt`, the `cmark` library target includes these source files:

```cmake
add_library(cmark
  blocks.c    buffer.c     cmark.c       cmark_ctype.c
  commonmark.c houdini_href_e.c houdini_html_e.c houdini_html_u.c
  html.c      inlines.c    iterator.c    latex.c
  man.c       node.c       references.c  render.c
  scanners.c  scanners.re  utf8.c        xml.c)
```

Target properties:
```cmake
set_target_properties(cmark PROPERTIES
  OUTPUT_NAME "cmark"
  PDB_NAME libcmark              # Avoid PDB name clash with executable
  POSITION_INDEPENDENT_CODE YES
  SOVERSION ${PROJECT_VERSION}   # Includes minor + patch in soname
  VERSION ${PROJECT_VERSION})
```

The library exposes headers via its interface include directories:
```cmake
target_include_directories(cmark INTERFACE
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
```

The export header is generated automatically:
```cmake
generate_export_header(cmark BASE_NAME ${PROJECT_NAME})
```

This produces `cmark_export.h` containing `CMARK_EXPORT` macros that resolve to `__declspec(dllexport/dllimport)` on Windows or `__attribute__((visibility("default")))` on Unix.

## Executable Target: `cmark_exe`

```cmake
add_executable(cmark_exe main.c)
set_target_properties(cmark_exe PROPERTIES
  OUTPUT_NAME "cmark"
  INSTALL_RPATH "${Base_rpath}")
target_link_libraries(cmark_exe PRIVATE cmark)
```

The executable has the same output name as the library (`cmark`), but the PDB names differ to avoid conflicts on Windows.

## Generated Files

Two files are generated at configure time:

### `cmark_version.h`

Generated from `cmark_version.h.in`:
```cmake
configure_file(cmark_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/cmark_version.h)
```

Contains `CMARK_VERSION` (integer) and `CMARK_VERSION_STRING` (string) macros.

### `libcmark.pc`

Generated from `libcmark.pc.in` for pkg-config integration:
```cmake
configure_file(libcmark.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libcmark.pc @ONLY)
```

## Test Infrastructure

Tests are enabled via CMake's standard `BUILD_TESTING` option (defaults to ON):

```cmake
if(BUILD_TESTING)
  add_subdirectory(api_test)
  add_subdirectory(test testdir)
endif()
```

### API Tests (`api_test/`)

C-level API tests that exercise the public API functions directly — node creation, manipulation, parsing, rendering.

### Spec Tests (`test/`)

CommonMark specification conformance tests. These parse expected input/output pairs from the CommonMark spec and verify cmark produces the correct output.

## RPATH Configuration

For shared library builds, the install RPATH is set to the library directory:

```cmake
if(BUILD_SHARED_LIBS)
  set(p "${CMAKE_INSTALL_FULL_LIBDIR}")
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${p}" i)
  if("${i}" STREQUAL "-1")
    set(Base_rpath "${p}")
  endif()
endif()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
```

This ensures the executable can find the shared library at runtime without requiring `LD_LIBRARY_PATH`.

## Man Page Generation

Man pages are built on non-Windows platforms:

```cmake
if(NOT CMAKE_SYSTEM_NAME STREQUAL Windows)
  add_subdirectory(man)
endif()
```

## Building for Fuzzing

To build the libFuzzer harness:

```bash
mkdir build-fuzz && cd build-fuzz
cmake -DCMARK_LIB_FUZZER=ON -DCMAKE_C_COMPILER=clang ..
make
```

The fuzz targets are in the `fuzz/` subdirectory.

## Platform-Specific Notes

### OpenBSD

The CLI tool uses `pledge(2)` on OpenBSD 6.0+ for sandboxing:
```c
#if defined(__OpenBSD__)
#  include <sys/param.h>
#  if OpenBSD >= 201605
#    define USE_PLEDGE
#    include <unistd.h>
#  endif
#endif
```

The pledge sequence is:
1. Before parsing: `pledge("stdio rpath", NULL)` — allows reading files
2. After parsing, before rendering: `pledge("stdio", NULL)` — drops file read access

### Windows

On Windows (non-Cygwin), binary mode is set for stdin/stdout to prevent CR/LF translation:
```c
#if defined(_WIN32) && !defined(__CYGWIN__)
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
```

## Scanner Regeneration

The `scanners.c` file is generated from `scanners.re` using re2c. To regenerate:

```bash
re2c --case-insensitive -b -i --no-generation-date -8 \
  -o scanners.c scanners.re
```

The generated file is checked into the repository, so re2c is not required for normal builds.

## Cross-References

- [cli-usage.md](cli-usage.md) — Command-line tool details and options
- [testing.md](testing.md) — Test framework details
- [code-style.md](code-style.md) — Coding conventions
- [scanner-system.md](scanner-system.md) — Scanner generation details
