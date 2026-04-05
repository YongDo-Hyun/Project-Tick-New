# MNV — Platform Support

## Officially Supported Platforms

MNV is designed for maximum portability.  The README states:

> MNV runs under MS-Windows (7, 8, 10, 11), macOS, Haiku, VMS and almost all
> flavours of UNIX.  Porting to other systems should not be very difficult.

### Tier 1: Actively tested in CI

| Platform | CI System | Build | GUI |
|---|---|---|---|
| Linux (Ubuntu/Debian) | GitHub Actions | CMake + Autoconf | GTK 3, terminal |
| macOS | GitHub Actions | CMake + Autoconf | terminal |
| Windows | Appveyor | MSVC (`Make_mvc.mak`) | Win32 native |
| FreeBSD | Cirrus CI | CMake + Autoconf | terminal |

### Tier 2: Supported, not CI-tested

| Platform | Notes |
|---|---|
| Other Linux distros | Community-packaged on Fedora, Arch, Gentoo, Alpine, etc. |
| Windows (MinGW/MSYS2) | `Make_ming.mak`, `Make_cyg_ming.mak` |
| Windows (Cygwin) | `Make_cyg.mak` with `iscygpty.c` PTY detection |
| Haiku | `gui_haiku.cc` backend, resource defs in `os_haiku.rdef.in` |
| OpenVMS | `Make_vms.mms`, `os_vms.c`, `os_vms_conf.h` |
| QNX | `os_qnx.c`, `gui_photon.c` |

### Tier 3: Historic / unmaintained

| Platform | Notes |
|---|---|
| MS-DOS | No longer maintained |
| Windows 95/98/Me/NT/2000/XP/Vista | Legacy; not tested |
| Amiga | `os_amiga.c`, `Make_ami.mak` — code present but unmaintained |
| Atari MiNT | Mentioned in `os_unix.c` header |
| BeOS | Haiku is the successor |
| RISC OS | Legacy |
| OS/2 | EMX support in `os_unix.c` |

---

## Platform Abstraction Strategy

MNV isolates platform-specific code in dedicated `os_*.c` / `os_*.h` files.
The core editor never calls raw system APIs directly — it uses wrappers
prefixed `mch_` ("machine"):

| Wrapper | Example implementations |
|---|---|
| `mch_early_init()` | Boot-time init: set signal handlers, console mode |
| `mch_exit()` | Clean exit with platform cleanup |
| `mch_fopen()` | `fopen()` with platform-specific path handling |
| `mch_signal()` | On Unix: `sigaction()`; elsewhere: `signal()` |
| `mch_getenv()` | Environment variable lookup |
| `mch_is_gui_executable()` | Win32: check subsystem header |

### Unix (`os_unix.c`, `os_unix.h`, `os_unixx.h`)

The largest platform file.  Covers:

- Signal handling (`SIGWINCH`, `SIGCHLD`, `SIGTSTP`, `SIGCONT`, …).
- Process control (`fork()`, `execvp()`, `waitpid()`).
- Terminal setup (`termios` / `termio` / `sgtty`).
- Pseudo-terminal allocation (`pty.c`).
- File locking and swap-file safety.
- SELinux context preservation (`HAVE_SELINUX`).
- Extended attribute support (`FEAT_XATTR`).
- XSMP (X Session Management Protocol) integration.
- Shared memory (`shm_open`) for IPC.

The file supports multiple Unix variants through conditionals:

```c
#if defined(__linux__) && !defined(__ANDROID__)
    // Linux-specific code
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
    // BSD-specific code
#endif
#if defined(__sun)
    // Solaris/SunOS (SUN_SYSTEM macro)
#endif
#if defined(__CYGWIN__)
    // Cygwin compatibility
#endif
```

### Windows (`os_win32.c`, `os_mswin.c`, `os_w32dll.c`, `os_w32exe.c`)

- `os_win32.c` — Console-mode Windows: console API, process spawning, pipe I/O.
- `os_mswin.c` — Shared code between console and GUI Windows builds.
- `os_w32dll.c` — Entry point when MNV is built as a DLL (`MNVDLL`).
- `os_w32exe.c` — Standard EXE entry point.
- `os_dos.h` — Legacy MS-DOS defines still used by Windows.

On Windows, `main()` is named `MNVMain` and the entry point may be in the DLL:

```c
#ifdef MSWIN
MNVMain
#else
main
#endif
(int argc, char **argv)
```

MinGW expands command-line arguments differently, so Windows builds call
`get_cmd_argsW()` for the raw wide-character argv.

### macOS (`os_mac_conv.c`, `os_macosx.m`, `os_mac.h`)

- `os_mac_conv.c` — Encoding conversion using Core Foundation.
- `os_macosx.m` — Objective-C bridge: pasteboard access, system services.
- `os_mac.h` — macOS-specific defines.

The `MACOS_X_DARWIN` / `MACOS_X` / `MACOS_CONVERT` macros control macOS
features.  Clipboard support uses Cocoa pasteboard via `FEAT_CLIPBOARD`.

### Haiku (`gui_haiku.cc`, `gui_haiku.h`, `os_haiku.h`, `os_haiku.rdef.in`)

Haiku support uses the native C++ Be API.  The `gui_haiku.cc` file is compiled
as C++ (the only `.cc` file in the codebase).  Resource definitions for the
application are in `os_haiku.rdef.in`.

### OpenVMS (`os_vms.c`, `os_vms_conf.h`, `os_vms_fix.com`, `os_vms_mms.c`)

VMS support includes:

- VMS-specific path handling (node::device:[directory]file.ext;version).
- `Make_vms.mms` — MMS/MMK build script.
- `os_vms_fix.com` — DCL post-processing script.

### Amiga (`os_amiga.c`, `os_amiga.h`)

Legacy Amiga support.  The code remains but is unmaintained.  `MNV_SIZEOF_INT`
is conditionally set for Amiga compilers:

```c
#ifdef AMIGA
# ifdef __GNUC__
#  define MNV_SIZEOF_INT 4
# else
#  define MNV_SIZEOF_INT 2
# endif
#endif
```

### QNX (`os_qnx.c`, `os_qnx.h`)

QNX-specific terminal and event handling.  The Photon GUI (`gui_photon.c`)
provides a native graphical interface on QNX.

---

## Display Server Support

### X11

Auto-detected by CMake via `find_package(X11)`.  When available:

- `HAVE_X11` is defined.
- X clipboard (`FEAT_CLIPBOARD`), client-server (`FEAT_CLIENTSERVER`), XIM
  input (`FEAT_XIM`), and XSMP session management (`FEAT_XSMP`) are enabled.
- Libraries linked: `libX11`, `libXt`, `libSM`, `libICE`.

### Wayland

Auto-detected via `pkg_check_modules(WAYLAND wayland-client)`.  When available:

- `HAVE_WAYLAND` / `FEAT_WAYLAND` are defined.
- `wayland-scanner` generates protocol stubs from XML files.
- Clipboard via `ext-data-control-v1`, `wlr-data-control-unstable-v1`, and
  optionally `xdg-shell` + `primary-selection-unstable-v1` for focus-stealing
  clipboard.
- The `vwl_connection_T` struct in `wayland.h` wraps the display, registry,
  seats, and global objects.

Wayland and X11 can coexist in the same build (e.g., an XWayland environment).

---

## Terminal Library

MNV requires a terminal library for console mode.  Detection order:

1. **ncurses** (`find_package(Curses)`) — preferred.  Sets `TERMINFO 1`.
2. **termcap / tinfo** (`find_library(NAMES termcap tinfo)`) — fallback.

The `tgetent()` function is checked to verify the library is usable:

```cmake
check_symbol_exists(tgetent "term.h" HAVE_TGETENT)
```

---

## Cygwin

`iscygpty.c` / `iscygpty.h` detect Cygwin pseudo-terminals so that MNV can
adjust its terminal handling:

```c
#if defined(MSWIN) && (!defined(FEAT_GUI_MSWIN) || defined(MNVDLL))
# include "iscygpty.h"
#endif
```

---

## Architecture and Word Size

MNV requires at least a 32-bit `int`:

```c
#if MNV_SIZEOF_INT < 4 && !defined(PROTO)
# error MNV only works with 32 bit int or larger
#endif
```

The build checks `sizeof(int)`, `sizeof(long)`, `sizeof(off_t)`,
`sizeof(time_t)`, and `sizeof(wchar_t)` at configure time:

```cmake
check_type_size(int      MNV_SIZEOF_INT)
check_type_size(long     MNV_SIZEOF_LONG)
check_type_size(off_t    SIZEOF_OFF_T)
check_type_size(time_t   SIZEOF_TIME_T)
check_type_size(wchar_t  SIZEOF_WCHAR_T)
```

If `sizeof(wchar_t) == 2` (Windows), the `SMALL_WCHAR_T` flag is set.

---

## Compiler Support

### GCC and Clang

Default warning flags:

```cmake
add_compile_options(-Wall -Wno-deprecated-declarations)
```

Release builds add:

```cmake
add_compile_options(-O2 -fno-strength-reduce)
```

The `HAVE_ATTRIBUTE_UNUSED` check enables `__attribute__((unused))` to
suppress warnings on intentionally unused parameters:

```c
#if defined(HAVE_ATTRIBUTE_UNUSED) || defined(__MINGW32__)
# define UNUSED __attribute__((unused))
#endif
```

### MSVC

Builds via `Make_mvc.mak` or CMake with Visual Studio generators.  Batch
helpers: `msvc-latest.bat`, `msvc2015.bat`, `msvc2017.bat`, `msvc2019.bat`,
`msvc2022.bat`.

### Other Compilers

Code contains conditionals for:

- **Aztec C** (Amiga): `#ifdef AZTEC_C`
- **SAS/C** (Amiga): `#ifdef SASC`
- **DCC** (Amiga): `#ifdef _DCC`
- **Tandem NonStop**: `#ifdef __TANDEM` — sets `ROOT_UID 65535`

---

## Packaging

MNV is packaged for many systems.  The `repology.org` badge tracks
distribution status.  Key packaging notes:

- Debian/Ubuntu provide `mnv`, `mnv-tiny`, and `mnv-gtk` variants.
- The `mnv.tiny` build uses `FEAT_TINY` as the default `vi`.
- Windows provides an NSIS-based installer (scripts in `nsis/`).
- macOS can be installed via Homebrew.
- The `mnvtutor.bat` (Windows) and `mnvtutor.com` (VMS) scripts launch the
  tutor on those platforms.

---

## Platform-Specific Installation Files

| File | Platform |
|---|---|
| `src/INSTALL` | Generic + Unix |
| `src/INSTALLami.txt` | Amiga |
| `src/INSTALLmac.txt` | macOS |
| `src/INSTALLpc.txt` | Windows |
| `src/INSTALLvms.txt` | OpenVMS |
| `src/INSTALLx.txt` | Cross-compilation |

---

*This document describes MNV 10.0 as of build 287 (2026-04-03).*
