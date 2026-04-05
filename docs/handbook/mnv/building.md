# MNV — Building

## Build System Overview

MNV ships with **two** fully functional build systems:

| System | Entry point | Status |
|---|---|---|
| CMake | `CMakeLists.txt` (project root) | **Primary** — recommended for new builds |
| Autoconf + Make | `src/configure` / `src/Makefile` | Legacy — widest platform coverage |

This document focuses on the CMake build but covers the Autoconf path as well.

---

## Prerequisites

### Compiler

MNV is written in C99.  Any modern C compiler works:

```cmake
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED OFF)
```

Tested compilers:
- GCC 7+
- Clang 7+
- MSVC 2015+ (via `Make_mvc.mak` or CMake generators)
- MinGW / MSYS2

### Required tools

| Tool | Minimum version |
|---|---|
| CMake | 3.15 |
| make / ninja | any recent version |
| pkg-config | recommended (for GTK, Wayland, libcanberra, libsodium) |

### Required libraries

Only a terminal library is strictly required:

- **ncurses** (preferred) or **termcap** / **tinfo**

Without it the build emits a warning:

```
No terminal library found (ncurses/curses/termcap). Build may fail.
```

### Optional libraries

| Library | CMake option | Feature |
|---|---|---|
| GTK 3 | `MNV_GUI=gtk3` | Graphical editor |
| GTK 2 | `MNV_GUI=gtk2` | Graphical editor (older) |
| Motif | `MNV_GUI=motif` | Motif-based GUI |
| X11 + Xt | auto-detected | X clipboard, client-server |
| Wayland + wayland-scanner | auto-detected | Wayland clipboard |
| libcanberra | `MNV_SOUND=ON` | Sound playback |
| libsodium | `MNV_SODIUM=ON` | XChaCha20 encryption |
| libacl | `MNV_ACL=ON` | POSIX ACL preservation |
| libgpm | `MNV_GPM=ON` | GPM mouse in console |
| Lua | `MNV_LUA=ON` | Lua scripting |
| Python 3 | `MNV_PYTHON3=ON` | Python 3 scripting |
| Ruby | `MNV_RUBY=ON` | Ruby scripting |
| Perl | `MNV_PERL=ON` | Perl scripting |
| Tcl | `MNV_TCL=ON` | Tcl scripting |
| MzScheme / Racket | `MNV_MZSCHEME=ON` | Scheme scripting |

### Ubuntu / Debian quick install

```bash
# Minimal build
sudo apt install git make cmake gcc libncurses-dev

# With X clipboard
sudo apt install libxt-dev

# With GTK 3 GUI
sudo apt install libgtk-3-dev

# With Wayland clipboard
sudo apt install libwayland-dev wayland-protocols wayland-scanner

# Optional interpreters
sudo apt install liblua5.4-dev libpython3-dev ruby-dev libperl-dev tcl-dev

# Optional features
sudo apt install libcanberra-dev libsodium-dev libacl1-dev libgpm-dev
```

---

## CMake Build — Quick Start

```bash
cd mnv
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
ctest                       # run tests
sudo cmake --install .      # install to /usr/local
```

---

## CMake Configuration Options

### Feature Level

```cmake
set(MNV_FEATURE "huge" CACHE STRING "Feature level: tiny, normal, huge")
```

| Level | Description |
|---|---|
| `tiny` | Minimal — no `+eval`, no terminal, no channels |
| `normal` | Default selection of features; includes `+eval` |
| `huge` | Everything: terminal, channels, NetBeans, cscope, beval, … |

On Unix/macOS/Windows the default is `huge`.

The CMake build translates the feature level into `config.h` defines consumed
by `feature.h`:

```cmake
if(MNV_FEATURE STREQUAL "huge")
    set(FEAT_HUGE 1)
    set(FEAT_EVAL 1)
    set(FEAT_BEVAL 1)
    set(FEAT_CSCOPE 1)
    set(FEAT_NETBEANS_INTG 1)
    set(FEAT_JOB_CHANNEL 1)
    set(FEAT_TERMINAL 1)
    set(FEAT_IPV6 1)
    ...
endif()
```

### GUI Selection

```cmake
set(MNV_GUI "auto" CACHE STRING "GUI toolkit: auto, gtk3, gtk2, motif, none")
```

- `auto` — tries GTK 3 → GTK 2 → Motif → none.
- `gtk3` / `gtk2` / `motif` — explicitly request one toolkit.
- `none` — terminal-only build.

When a GUI is found, these variables are set and the corresponding source files
are added:

```cmake
# GTK 3 example
set(GUI_SRC gui.c gui_gtk.c gui_gtk_f.c gui_gtk_x11.c gui_beval.c)
```

### Boolean Options

| Option | Default | Effect |
|---|---|---|
| `MNV_TERMINAL` | `ON` | Built-in terminal emulator (requires channels) |
| `MNV_CHANNEL` | `ON` | Job/channel support |
| `MNV_NETBEANS` | `ON` | NetBeans interface |
| `MNV_CSCOPE` | `ON` | Cscope integration |
| `MNV_SOUND` | `ON` | Sound via libcanberra |
| `MNV_ACL` | `ON` | POSIX ACL support |
| `MNV_GPM` | `ON` | GPM mouse |
| `MNV_SODIUM` | `ON` | libsodium encryption |
| `MNV_MULTIBYTE` | `ON` | Multi-byte character support |
| `MNV_XIM` | `ON` | X Input Method |

### Language Interpreters

All interpreters default to `OFF` and can be loaded dynamically:

```cmake
option(MNV_LUA         "Enable Lua interpreter"        OFF)
option(MNV_LUA_DYNAMIC "Load Lua dynamically"          ON)
option(MNV_PERL        "Enable Perl interpreter"       OFF)
option(MNV_PERL_DYNAMIC "Load Perl dynamically"        ON)
option(MNV_PYTHON3     "Enable Python 3 interpreter"   OFF)
option(MNV_PYTHON3_DYNAMIC "Load Python 3 dynamically" ON)
option(MNV_RUBY        "Enable Ruby interpreter"       OFF)
option(MNV_RUBY_DYNAMIC "Load Ruby dynamically"        ON)
option(MNV_TCL         "Enable Tcl interpreter"        OFF)
option(MNV_TCL_DYNAMIC "Load Tcl dynamically"          ON)
option(MNV_MZSCHEME    "Enable MzScheme/Racket"        OFF)
```

Dynamic loading means MNV `dlopen()`s the interpreter shared library at
runtime instead of linking against it at build time.  The library name is
auto-detected:

```cmake
foreach(_lua_lib ${LUA_LIBRARIES})
    if(_lua_lib MATCHES "\\.(so|dylib)")
        get_filename_component(DYNAMIC_LUA_DLL "${_lua_realpath}" NAME)
        break()
    endif()
endforeach()
```

### Development Options

```cmake
option(MNV_DEBUG       "Enable debug build"                    OFF)
option(MNV_PROFILE     "Enable profiling"                      OFF)
option(MNV_SANITIZE    "Enable address/undefined sanitizers"   OFF)
option(MNV_LEAK_CHECK  "Enable EXITFREE for leak checking"     OFF)
option(MNV_BUILD_TESTS "Build and enable tests"                ON)
```

When `MNV_DEBUG` is on:

```cmake
add_compile_options(-g -DDEBUG)
```

When `MNV_SANITIZE` is on:

```cmake
add_compile_options(
    -fsanitize=address
    -fsanitize=undefined
    -fsanitize-recover=all
    -fno-omit-frame-pointer
)
add_link_options(
    -fsanitize=address
    -fsanitize=undefined
)
```

When `MNV_LEAK_CHECK` is on, the `EXITFREE` define ensures all memory is freed
at exit so leak checkers can report accurately.

### Compiled-by string

```cmake
set(MNV_COMPILED_BY "" CACHE STRING "Name of the person compiling MNV")
```

Embedded into `auto/pathdef.c` and shown in `:version` output.

---

## CMake Presets

`CMakePresets.json` defines ready-made configurations.  Use them with:

```bash
cmake --preset <name>
cmake --build --preset <name>
```

### Available presets

| Preset | Feature | GUI | Interpreters | Debug |
|---|---|---|---|---|
| `default` | huge | auto | none | no |
| `minimal` | tiny | none | none | no |
| `normal` | normal | none | none | no |
| `nogui` | huge | none | none | no |
| `gtk3` | huge | gtk3 | none | no |
| `all-interp` | huge | auto | all (dynamic) | yes |
| `all-interp-static` | huge | auto | all (static) | yes |
| `lua-only` | huge | none | Lua (dynamic) | yes |
| `python3-only` | huge | none | Python3 (dynamic) | yes |
| `ruby-only` | huge | none | Ruby (dynamic) | yes |
| `perl-only` | huge | none | Perl (dynamic) | yes |
| `tcl-only` | huge | none | Tcl (dynamic) | yes |
| `sanitize` | huge | none | none | ASan+UBSan |
| `profile` | huge | none | none | gprof |

Presets inherit from hidden base presets:

- **`base`** — sets `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, `MNV_BUILD_TESTS=ON`,
  output to `build/${presetName}`.
- **`dev-base`** — inherits `base`, adds `MNV_DEBUG=ON`, `MNV_LEAK_CHECK=ON`.

### Example: debug build with all interpreters

```bash
cmake --preset all-interp
cmake --build build/all-interp -j$(nproc)
cd build/all-interp && ctest
```

### Example: sanitiser build

```bash
cmake --preset sanitize
cmake --build build/sanitize -j$(nproc)
cd build/sanitize && ctest
```

---

## System Detection

The CMake build performs extensive detection of headers, functions, and types.

### Header checks

```cmake
check_include_file(stdint.h      HAVE_STDINT_H)
check_include_file(unistd.h      HAVE_UNISTD_H)
check_include_file(termios.h     HAVE_TERMIOS_H)
check_include_file(sys/select.h  HAVE_SYS_SELECT_H)
check_include_file(dlfcn.h       HAVE_DLFCN_H)
check_include_file(pthread.h     HAVE_PTHREAD_H)
# ... 40+ more
```

### Function checks

```cmake
check_function_exists(fchdir   HAVE_FCHDIR)
check_function_exists(getcwd   HAVE_GETCWD)
check_function_exists(select   HAVE_SELECT)
check_function_exists(sigaction HAVE_SIGACTION)
check_function_exists(dlopen   HAVE_DLOPEN)
# ... 50+ more
```

### Type-size checks

```cmake
check_type_size(int      MNV_SIZEOF_INT)
check_type_size(long     MNV_SIZEOF_LONG)
check_type_size(off_t    SIZEOF_OFF_T)
check_type_size(time_t   SIZEOF_TIME_T)
check_type_size(wchar_t  SIZEOF_WCHAR_T)
```

MNV requires `sizeof(int) >= 4`:

```c
#if MNV_SIZEOF_INT < 4 && !defined(PROTO)
# error MNV only works with 32 bit int or larger
#endif
```

### Compile tests

```cmake
# __attribute__((unused))
check_c_source_compiles("
    int x __attribute__((unused));
    int main() { return 0; }
" HAVE_ATTRIBUTE_UNUSED)

# struct stat.st_blksize
check_c_source_compiles("
    #include <sys/stat.h>
    int main() { struct stat s; s.st_blksize = 0; return 0; }
" HAVE_ST_BLKSIZE)

# sockaddr_un
check_c_source_compiles("
    #include <sys/un.h>
    int main() { struct sockaddr_un s; return 0; }
" HAVE_SOCKADDR_UN)
```

---

## Generated Files

### `auto/config.h`

Generated from `cmake/config.h.cmake` by `configure_file()`.  Contains all
`HAVE_*`, `FEAT_*`, `SIZEOF_*`, and `DYNAMIC_*` defines.

### `auto/pathdef.c`

Generated from `cmake/pathdef.c.cmake`.  Embeds:

- Compiler used (`MNV_ALL_CFLAGS`)
- Linker used (`MNV_ALL_LFLAGS`)
- Compiled-by info (`MNV_COMPILED_USER@MNV_COMPILED_SYS`)
- Install paths for runtime files

### `auto/osdef.h`

On modern systems this is a stub:

```c
/* osdef.h - generated by CMake */
/* On modern systems most definitions come from system headers */
```

The Autoconf build generates a real `osdef.h` from `osdef.sh`.

### Wayland protocol files

When Wayland is detected and `wayland-scanner` is available, the build
generates `.c` and `.h` files for each supported protocol:

```cmake
set(WAYLAND_PROTOCOLS
    ext-data-control-v1
    primary-selection-unstable-v1
    wlr-data-control-unstable-v1
    xdg-shell
)
```

Protocol XMLs must exist under `src/auto/wayland/protocols/`.

---

## Build Targets

### `mnv` (main executable)

All `MNV_CORE_SRC` files + `xdiff` object library → single binary.

```cmake
add_executable(mnv ${MNV_CORE_SRC} $<TARGET_OBJECTS:xdiff>)
```

### `xdiff` (object library)

```cmake
add_library(xdiff OBJECT
    src/xdiff/xdiffi.c
    src/xdiff/xemit.c
    src/xdiff/xprepare.c
    src/xdiff/xutils.c
    src/xdiff/xhistogram.c
    src/xdiff/xpatience.c
)
```

### `vterm` (static library)

Built when `FEAT_TERMINAL` is enabled:

```cmake
add_subdirectory(src/libvterm)
```

### `xxd` (executable)

Hex dump utility:

```cmake
add_subdirectory(src/xxd)
```

### Unit tests

Four test executables built by `mnv_add_unit_test()`:

```cmake
mnv_add_unit_test(json_test    json.c     json_test.c)
mnv_add_unit_test(kword_test   charset.c  kword_test.c)
mnv_add_unit_test(memfile_test memfile.c  memfile_test.c)
mnv_add_unit_test(message_test message.c  message_test.c)
```

Each replaces `main.c` **and** the file under test with its `_test.c` variant.

---

## Testing

### Running all tests

```bash
cd build && ctest
```

### Specific test categories

```bash
ctest -L scripts    # Script tests (src/testdir/)
ctest -L indent     # Indent tests (runtime/indent/)
ctest -L syntax     # Syntax tests (runtime/syntax/)
ctest -R json_test  # Just the JSON unit test
```

### Test environment

Tests run with explicit environment to avoid GUI interference:

```cmake
set_tests_properties(${TEST_NAME} PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY="
)
```

Script tests use:

```cmake
MNVPROG=$<TARGET_FILE:mnv>
MNVRUNTIME=${CMAKE_CURRENT_SOURCE_DIR}/runtime
LINES=24
COLUMNS=80
```

---

## Installation

```bash
sudo cmake --install build
# Default prefix: /usr/local
```

### What gets installed

| Component | Destination |
|---|---|
| `mnv` binary | `${CMAKE_INSTALL_BINDIR}` (e.g. `/usr/local/bin`) |
| Symlinks (ex, view, vi, vim, rmnv, rview, mnvdiff) | same |
| GUI symlinks (gmnv, gview, gvi, gvim, …) | same (if GUI) |
| Runtime files | `${CMAKE_INSTALL_DATADIR}/mnv/mnv100/` |
| Man pages | `${CMAKE_INSTALL_MANDIR}/man1/` |
| Desktop files | `${CMAKE_INSTALL_DATADIR}/applications/` (if GUI) |
| Icons | `${CMAKE_INSTALL_DATADIR}/icons/hicolor/.../` (if GUI) |

### Runtime subdirectories installed

```cmake
set(RUNTIME_SUBDIRS
    colors compiler doc ftplugin import indent keymap
    lang macros pack plugin print spell syntax tools
    tutor autoload
)
```

### Individual runtime scripts installed

```cmake
set(RUNTIME_SCRIPTS
    defaults.mnv emnv.mnv filetype.mnv ftoff.mnv ftplugin.mnv
    ftplugof.mnv indent.mnv indoff.mnv menu.mnv mswin.mnv
    optwin.mnv bugreport.mnv scripts.mnv synmenu.mnv delmenu.mnv
)
```

---

## Legacy Autoconf Build

```bash
cd mnv/src
./configure --with-features=huge --enable-gui=gtk3
make -j$(nproc)
make test
sudo make install
```

Key configure flags:

| Flag | Effect |
|---|---|
| `--with-features=tiny\|normal\|huge` | Feature level |
| `--enable-gui=gtk3\|gtk2\|motif\|no` | GUI selection |
| `--enable-luainterp=dynamic` | Lua support |
| `--enable-python3interp=dynamic` | Python 3 support |
| `--enable-rubyinterp=dynamic` | Ruby support |
| `--enable-perlinterp=dynamic` | Perl support |
| `--enable-tclinterp=dynamic` | Tcl support |

Platform-specific Make files:

| File | Platform |
|---|---|
| `Make_cyg.mak` | Cygwin |
| `Make_cyg_ming.mak` | Cygwin + MinGW |
| `Make_ming.mak` | MinGW |
| `Make_mvc.mak` | MSVC |
| `Make_ami.mak` | Amiga |
| `Make_vms.mms` | OpenVMS (MMS/MMK) |

---

## Build Summary

At the end of configuration, CMake prints a summary:

```
=== MNV 10.0 Build Configuration ===
  Feature level:    huge
  GUI:              auto (found: TRUE)
  Terminal:         1
  Channel/Job:      1
  X11:              1
  Wayland:          1
  Terminal lib:     /usr/lib/x86_64-linux-gnu/libncurses.so
  Sound:            canberra
  Encryption:       libsodium
  Lua:              5.4.6
  Python 3:         3.12.3
  Install prefix:   /usr/local
  Compiled by:      user@hostname
=============================================
```

---

## Cross-Compilation

See `src/INSTALLx.txt` for cross-compile instructions.  With CMake, use a
toolchain file:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake
```

The legacy build uses `toolchain-mingw32.cmake` (from `cmark/`) as a reference.

---

## Troubleshooting

| Problem | Solution |
|---|---|
| "No terminal library found" | Install `libncurses-dev` (Debian/Ubuntu) or `ncurses-devel` (RHEL/Fedora) |
| GUI not detected | Install `libgtk-3-dev` and ensure `pkg-config` is available |
| `configure did not run properly` | Autoconf build: check `auto/config.log` |
| Perl `xsubpp` not found | Install `perl-ExtUtils-MakeMaker` or the full Perl dev package |
| Wayland protocols missing | Ensure protocol XML files exist in `src/auto/wayland/protocols/` |
| `MNV only works with 32 bit int or larger` | Your platform has 16-bit int — unsupported |

---

*This document describes MNV 10.0 as of build 287 (2026-04-03).*
