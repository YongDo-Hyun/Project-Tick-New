# PTlibzippy

## Overview

PTlibzippy is a Project Tick fork of the [zlib](https://zlib.net/) data compression library.
It is a general-purpose lossless data compression library implementing the DEFLATE algorithm
as specified in RFCs 1950 (zlib format), 1951 (deflate format), and 1952 (gzip format).

PTlibzippy was version 0.0.5.1 and was maintained as a detached fork to solve symbol
conflicts when bundling zlib alongside libpng in the ProjT Launcher's build system.

**Status**: Archived — system zlib is now used instead of a bundled fork.

---

## Project Identity

| Property          | Value                                                    |
|-------------------|----------------------------------------------------------|
| **Name**          | PTlibzippy                                               |
| **Location**      | `archived/ptlibzippy/`                                   |
| **Language**       | C                                                       |
| **Version**       | 0.0.5.1                                                  |
| **License**       | zlib license (permissive)                                |
| **Copyright**     | 1995–2026 Jean-loup Gailly and Mark Adler; 2026 Project Tick |
| **Homepage**      | https://projecttick.org/p/zlib                           |
| **FAQ**           | https://projecttick.org/p/zlib/zlib_faq.html             |
| **Contact**       | community@community.projecttick.org                      |

---

## Why a zlib Fork?

The fork was created to solve a specific technical problem in the ProjT Launcher:

### The Symbol Conflict Problem

When the launcher bundled both zlib and libpng as static libraries, the linker
encountered duplicate symbol definitions. Both zlib and libpng's internal zlib
usage exported identical function names (e.g., `deflate`, `inflate`, `compress`),
causing link-time errors or runtime symbol resolution ambiguities.

### The Solution

PTlibzippy addressed this through:

1. **Symbol prefixing** — The `PTLIBZIPPY_PREFIX` CMake option enables renaming all
   public symbols with a custom prefix, preventing collisions
2. **PNG shim layer** — A custom `ptlibzippy_pngshim.c` file provided a compatibility
   layer between the renamed zlib symbols and libpng's expectations
3. **Custom header names** — Headers were renamed (`ptlibzippy.h`, `ptzippyconf.h`,
   `ptzippyguts.h`, `ptzippyutil.h`) to avoid include-path conflicts

As noted in the ProjT Launcher changelog:
> "zlib symbol handling was refined to use libpng-targeted shim overrides instead
> of global prefixing."

---

## License

The zlib license is permissive and compatible with GPL:

```
Copyright notice:

 (C) 1995-2026 Jean-loup Gailly and Mark Adler
 (C) 2026 Project Tick

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
```

---

## Build System

PTlibzippy supported three build systems:

### CMake (Primary)

```cmake
cmake_minimum_required(VERSION 3.12...3.31)

project(
    PTlibzippy
    LANGUAGES C
    VERSION 0.0.5.1
    HOMEPAGE_URL "https://projecttick.org/p/zlib"
    DESCRIPTION "PTlibzippy - a general-purpose lossless data-compression library")
```

#### CMake Options

| Option                     | Default | Description                                  |
|---------------------------|---------|----------------------------------------------|
| `PTLIBZIPPY_BUILD_TESTING`| `ON`    | Enable building test programs                |
| `PTLIBZIPPY_BUILD_SHARED` | `ON`    | Build shared library (`libptlibzippy.so`)    |
| `PTLIBZIPPY_BUILD_STATIC` | `ON`    | Build static library (`libptlibzippystatic.a`)|
| `PTLIBZIPPY_INSTALL`      | `ON`    | Enable `make install` target                 |
| `PTLIBZIPPY_PREFIX`       | `OFF`   | Enable symbol prefixing for all public APIs  |

#### Feature Detection

The CMake build detected platform capabilities:

```cmake
check_type_size(off64_t OFF64_T)          # Large file support
check_function_exists(fseeko HAVE_FSEEKO) # POSIX fseeko
check_include_file(stdarg.h HAVE_STDARG_H)
check_include_file(unistd.h HAVE_UNISTD_H)
```

And generated `ptzippyconf.h` with the results:

```cmake
configure_file(${PTlibzippy_BINARY_DIR}/ptzippyconf.h.cmakein
               ${PTlibzippy_BINARY_DIR}/ptzippyconf.h)
```

#### Visibility Attributes

On non-Windows platforms, the build checked for GCC visibility attributes:

```c
void f(void) __attribute__ ((visibility("hidden")));
```

This enabled hiding internal symbols from the shared library's public API.

#### Library Targets

**Shared library:**

```cmake
add_library(ptlibzippy SHARED ${PTLIBZIPPY_SRCS} ...)
add_library(PTlibzippy::PTlibzippy ALIAS ptlibzippy)

target_compile_definitions(ptlibzippy
    PRIVATE PTLIBZIPPY_BUILD PTLIBZIPPY_INTERNAL= ...)
```

**Static library:**

```cmake
add_library(ptlibzippystatic STATIC ${PTLIBZIPPY_SRCS} ...)
```

On Windows, the static library gets a `"s"` suffix to distinguish from the import library.

#### pkg-config

A `ptlibzippy.pc` file was generated for pkg-config integration:

```cmake
configure_file(${PTlibzippy_SOURCE_DIR}/ptlibzippy.pc.cmakein
               ${PTLIBZIPPY_PC} @ONLY)
```

### Autotools

Traditional Unix build:

```bash
./configure
make
make test
make install
```

### Bazel

```
BUILD.bazel       # Build rules
MODULE.bazel      # Module definition
```

---

## Source Files

### Public Headers

| Header          | Purpose                                            |
|----------------|----------------------------------------------------|
| `ptlibzippy.h` | Public API (compress, decompress, gzip, etc.)      |
| `ptzippyconf.h`| Configuration header (generated at build time)     |

### Private Headers

| Header          | Purpose                                            |
|----------------|----------------------------------------------------|
| `crc32.h`       | CRC-32 lookup tables                              |
| `deflate.h`     | DEFLATE compression state machine                 |
| `ptzippyguts.h` | Internal definitions (gzip state)                  |
| `inffast.h`     | Fast inflate inner loop                           |
| `inffixed.h`    | Fixed Huffman code tables                         |
| `inflate.h`     | Inflate state machine                             |
| `inftrees.h`    | Huffman tree building                             |
| `trees.h`       | Dynamic Huffman tree encoding                     |
| `ptzippyutil.h` | System-level utilities                            |

### Source Files

| Source                   | Purpose                                       |
|-------------------------|-----------------------------------------------|
| `adler32.c`             | Adler-32 checksum computation                 |
| `compress.c`            | Compression convenience API                   |
| `crc32.c`               | CRC-32 checksum computation                   |
| `deflate.c`             | DEFLATE compression algorithm                 |
| `gzclose.c`             | gzip file close                               |
| `gzlib.c`               | gzip file utility functions                   |
| `gzread.c`              | gzip file reading                             |
| `gzwrite.c`             | gzip file writing                             |
| `inflate.c`             | DEFLATE decompression algorithm               |
| `infback.c`             | Inflate using a callback interface            |
| `inftrees.c`            | Generate Huffman trees for inflate            |
| `inffast.c`             | Fast inner loop for inflate                   |
| `ptlibzippy_pngshim.c` | PNG integration shim (Project Tick addition)  |
| `trees.c`               | Output compressed data using Huffman coding   |
| `uncompr.c`             | Decompression convenience API                 |
| `ptzippyutil.c`         | Operating system interface utilities          |

### Project Tick Additions

The following files were added or renamed by Project Tick (not present in upstream zlib):

| File                     | Change Type | Purpose                              |
|-------------------------|-------------|--------------------------------------|
| `ptlibzippy_pngshim.c`  | Added       | Shim for libpng symbol compatibility |
| `ptzippyguts.h`          | Renamed     | From `gzguts.h`                     |
| `ptzippyutil.c`          | Renamed     | From `zutil.c`                      |
| `ptzippyutil.h`          | Renamed     | From `zutil.h`                      |
| `ptzippyconf.h`          | Renamed     | From `zconf.h`                      |
| `ptlibzippy.h`           | Renamed     | From `zlib.h`                       |
| `ptlibzippy.pc.cmakein`  | Renamed     | From `zlib.pc.cmakein`              |
| `COPYING.md`             | Modified    | Added Project Tick copyright         |

---

## Symbol Prefixing

The `PTLIBZIPPY_PREFIX` option enables symbol prefixing for all public API functions.
When enabled, all zlib functions are prefixed to avoid collisions:

| Original Symbol | Prefixed Symbol (example) |
|----------------|---------------------------|
| `deflate`       | `pt_deflate`             |
| `inflate`       | `pt_inflate`             |
| `compress`      | `pt_compress`            |
| `uncompress`    | `pt_uncompress`          |
| `crc32`         | `pt_crc32`               |
| `adler32`       | `pt_adler32`             |

The prefix is configured through `ptzippyconf.h`:

```cmake
set(PT_PREFIX ${PTLIBZIPPY_PREFIX})
file(APPEND ${PTCONF_OUT_FILE} "#cmakedefine PT_PREFIX 1\n")
```

---

## PNG Shim Layer

The `ptlibzippy_pngshim.c` file was the key Project Tick addition. It provided a
compatibility layer that allowed libpng to use PTlibzippy's renamed symbols
transparently.

Without the shim, libpng would look for standard zlib function names (`deflate`,
`inflate`, etc.) and fail to link against PTlibzippy's prefixed versions.

The shim worked by:
1. Including PTlibzippy's headers (with prefixed symbols)
2. Providing wrapper functions with the original zlib names
3. Each wrapper forwarded to the corresponding PTlibzippy function

This approach was described in the changelog as:
> "zlib symbol handling was refined to use libpng-targeted shim overrides instead
> of global prefixing"

---

## Cross-Platform Support

PTlibzippy inherited zlib's extensive platform support:

| Platform          | Build System                  | Notes                         |
|------------------|-------------------------------|-------------------------------|
| Linux            | CMake, Autotools, Makefile    | Primary development platform  |
| macOS            | CMake, Autotools              |                               |
| Windows          | CMake, NMake, MSVC            | DLL and static library        |
| Windows (MinGW)  | CMake, Makefile               |                               |
| Cygwin           | CMake, Autotools              | DLL naming handled            |
| Amiga            | Makefile.pup, Makefile.sas    | SAS/C compiler                |
| OS/400           | Custom makefiles              | IBM i (formerly AS/400)       |
| QNX              | Custom makefiles              | QNX Neutrino                  |
| VMS              | make_vms.com                  | OpenVMS command procedure     |

### Platform-Specific Notes from README

- **64-bit Irix**: `deflate.c` must be compiled without optimization with `-O`
- **Digital Unix 4.0D**: Requires `cc -std1` for correct `gzprintf` behavior
- **HP-UX 9.05**: Some versions of `/bin/cc` are incompatible
- **PalmOS**: Supported via external port (https://palmzlib.sourceforge.net/)

---

## Third-Party Contributions

The `contrib/` directory contained community-contributed extensions:

| Directory       | Description                                    |
|----------------|------------------------------------------------|
| `contrib/ada/`  | Ada programming language bindings             |
| `contrib/blast/`| PKWare Data Compression Library decompressor  |
| `contrib/crc32vx/`| Vectorized CRC-32 for IBM z/Architecture    |
| `contrib/delphi/`| Borland Delphi bindings                      |
| `contrib/dotzlib/`| .NET (C#) bindings                           |
| `contrib/gcc_gvmat64/`| x86-64 assembly optimizations             |

Ada bindings included full package specifications:

```
contrib/ada/ptlib.ads       # Package spec
contrib/ada/ptlib.adb       # Package body
contrib/ada/ptlib-thin.ads  # Thin binding spec
contrib/ada/ptlib-thin.adb  # Thin binding body
contrib/ada/ptlib-streams.ads # Stream interface spec
contrib/ada/ptlib-streams.adb # Stream interface body
```

---

## FAQ Highlights

From the project FAQ:

**Q: Is PTlibzippy Y2K-compliant?**
A: Yes. PTlibzippy doesn't handle dates.

**Q: Can zlib handle .zip archives?**
A: Not by itself. See `contrib/minizip`.

**Q: Can zlib handle .Z files?**
A: No. Use `uncompress` or `gunzip` subprocess.

**Q: How can I make a Unix shared library?**
A: Default build produces shared + static libraries:
```bash
make distclean
./configure
make
```

---

## Language Bindings

PTlibzippy (and its zlib base) was accessible from many languages:

| Language  | Interface                                         |
|----------|---------------------------------------------------|
| C        | Native API via `ptlibzippy.h`                     |
| C++      | Direct C API usage                                |
| Ada      | `contrib/ada/` bindings                           |
| C# (.NET)| `contrib/dotzlib/` bindings                       |
| Delphi   | `contrib/delphi/` bindings                        |
| Java     | `java.util.zip` package (JDK built-in)            |
| Perl     | IO::Compress module                              |
| Python   | `zlib` module (Python standard library)           |
| Tcl      | Built-in zlib support                             |

---

## Integration with ProjT Launcher

In the ProjT Launcher's CMake build, PTlibzippy was used via:

```cmake
# From cmake/usePTlibzippy.cmake (referenced in the launcher's cmake/ directory)
```

The launcher's `CMakeLists.txt` imported PTlibzippy alongside other compression
libraries (bzip2, quazip) to handle:

- Mod archive extraction (`.zip`, `.jar` files)
- Instance backup/restore
- Asset pack handling
- Modpack import/export (Modrinth `.mrpack`, CurseForge `.zip` formats)

---

## Why It Was Archived

PTlibzippy was archived when:

1. **Symbol conflict resolution matured** — The launcher's build system evolved to
   handle zlib/libpng coexistence without a custom fork
2. **System zlib preferred** — Using the system's zlib package reduced maintenance
   burden and ensured security patches were applied promptly
3. **Launcher archived** — When ProjT Launcher was archived, its dependency libraries
   (including PTlibzippy) were archived alongside it
4. **MeshMC approach** — The successor launcher (MeshMC) uses system libraries or
   vendored sources with different conflict resolution strategies

---

## Building (for Reference)

### CMake

```bash
cd archived/ptlibzippy/
mkdir build && cd build
cmake ..
make
make test
```

### With Symbol Prefixing

```bash
cmake .. -DPTLIBZIPPY_PREFIX=pt_
make
```

### Autotools

```bash
cd archived/ptlibzippy/
./configure
make
make test
make install
```

### Static-Only Build

```bash
cmake .. -DPTLIBZIPPY_BUILD_SHARED=OFF
make
```

Note: Build success is not guaranteed since the archived code is not maintained.

---

## File Index

From the project `INDEX` file:

```
CMakeLists.txt     cmake build file
ChangeLog          history of changes
FAQ                Frequently Asked Questions about zlib
INDEX              file listing
Makefile           dummy Makefile that tells you to ./configure
Makefile.in        template for Unix Makefile
README             project overview
configure          configure script for Unix
make_vms.com       makefile for VMS
test/example.c     zlib usage examples for build testing
test/minigzip.c    minimal gzip-like functionality for build testing
test/infcover.c    inf*.c code coverage for build coverage testing
treebuild.xml      XML description of source file dependencies
ptzippyconf.h      zlib configuration header (template)
ptlibzippy.h       zlib public API header

amiga/             makefiles for Amiga SAS C
doc/               documentation for formats and algorithms
msdos/             makefiles for MSDOS
old/               legacy makefiles and documentation
os400/             makefiles for OS/400
qnx/               makefiles for QNX
watcom/            makefiles for OpenWatcom
win32/             makefiles for Windows
```

---

## Ownership

Maintained by `@YongDo-Hyun` as defined in `ci/OWNERS`:

```
/archived/ptlibzippy/                                @YongDo-Hyun
```
