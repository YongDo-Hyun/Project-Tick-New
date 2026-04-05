# MNV — Architecture

## High-Level Organisation

The MNV source tree lives under `mnv/` inside the Project Tick monorepo.
It follows a flat layout: the editor's core C sources and headers are in
`mnv/src/`, build infrastructure at the project root, and runtime files under
`mnv/runtime/`.

```
mnv/
├── CMakeLists.txt          # Primary CMake build
├── CMakePresets.json        # Named build presets
├── Makefile                 # Legacy top-level Make wrapper
├── configure                # Autoconf-generated configure script
├── src/                     # All C source and headers
│   ├── auto/                # Generated files (config.h, osdef.h, pathdef.c)
│   ├── libvterm/            # Embedded terminal emulation library
│   ├── xdiff/               # Embedded diff library
│   ├── xxd/                 # Hex-dump utility (separate binary)
│   ├── proto/               # Function prototypes (*.pro files)
│   ├── testdir/             # Script-driven test suite
│   └── *.c / *.h            # Editor source files
├── runtime/                 # Scripts, docs, syntax, ftplugins, …
├── ci/                      # CI helper scripts
├── cmake/                   # CMake template files
├── nsis/                    # Windows installer scripts
├── pixmaps/                 # Application icons
├── tools/                   # Developer tools
└── lang/                    # Source-level gettext catalogs
```

---

## The Compilation Unit Model

MNV is a single-binary C project.  There is **one** executable target (`mnv`)
comprising roughly 100 `.c` files compiled together with a shared set of
headers.  The primary header is `src/mnv.h`, which every `.c` file includes:

```c
#include "mnv.h"
```

`mnv.h` in turn pulls in the header cascade:

```
mnv.h
 ├── protodef.h          — helper macros for function prototypes
 ├── feature.h           — feature-level #defines (FEAT_TINY/NORMAL/HUGE)
 ├── os_unix.h / os_win32.h / os_amiga.h  — platform headers
 ├── ascii.h             — ASCII character constants
 ├── keymap.h            — terminal key-code mapping
 ├── termdefs.h          — terminal capability definitions
 ├── macros.h            — utility macros
 ├── option.h            — option flag definitions
 ├── beval.h             — balloon-eval declarations
 ├── proto.h             — master prototype include (pulls every src/proto/*.pro)
 ├── structs.h           — all major struct typedefs
 ├── globals.h           — EXTERN global variables
 ├── errors.h            — error message externs
 └── gui.h               — GUI struct and macro definitions
```

The `EXTERN` macro (defined in `globals.h`) is used to declare global variables
in headers.  `main.c` defines `#define EXTERN` before including `mnv.h`, which
causes the globals to be **defined** there and merely **declared** everywhere
else.

```c
// main.c
#define EXTERN
#include "mnv.h"
```

---

## Core Subsystem Map

Below is every `.c` file in `src/` grouped by subsystem, with a description of
its role.

### Entry Point

| File | Role |
|---|---|
| `main.c` | Program entry.  Contains `main()` (or `MNVMain()` on Windows).  Parses command-line arguments via `command_line_scan()`, runs `common_init_1()` / `common_init_2()`, sources startup scripts, creates windows, and enters the main editing loop.  Key functions: `mainerr()`, `early_arg_scan()`, `usage()`, `parse_command_name()`, `check_tty()`, `read_stdin()`, `create_windows()`, `edit_buffers()`, `exe_pre_commands()`, `exe_commands()`, `source_startup_scripts()`, `main_start_gui()`. |
| `version.c` | Version number, patch list, build date. Included patches tracked in `included_patches[]`. |
| `version.h` | Macros: `MNV_VERSION_MAJOR`, `MNV_VERSION_MINOR`, `MNV_VERSION_BUILD`, `MNV_VERSION_LONG`. |

### Memory Management

| File | Role |
|---|---|
| `alloc.c` | `malloc`/`realloc`/`free` wrappers: `alloc()`, `alloc_clear()`, `mnv_free()`, `mnv_realloc()`.  Optional `MEM_PROFILE` support for tracking allocations.  Global counters: `mem_allocs[]`, `mem_frees[]`, `mem_allocated`, `mem_freed`, `mem_peak`. |
| `alloc.h` | Allocation function prototypes. |
| `gc.c` | Garbage collection for MNV9 objects. |

### Buffer and Window Management

| File | Role |
|---|---|
| `buffer.c` | Buffer list management.  Double-linked list of `buf_T` structs.  States: never-loaded / not-loaded / hidden / normal.  Functions: `buflist_add()`, `buflist_findname()`, `enter_buffer()`, `do_buffer()`, `free_buffer()`, `trigger_undo_ftplugin()`. |
| `window.c` | Window split, close, resize, and navigation.  Manages `win_T` linked list and `frame_T` layout tree. |
| `tabpanel.c` | Tab page panel display logic. |

### Text Storage

| File | Role |
|---|---|
| `memline.c` | Memory line — the B-tree layer that stores buffer lines on disk in swap files and reads them back on demand.  Operates on `memfile_T` structures. |
| `memfile.c` | Block-level swap-file I/O.  Manages memory-file pages. |

### Editing Operations

| File | Role |
|---|---|
| `edit.c` | Insert-mode handling — character insertion, completion triggers, abbreviations. |
| `change.c` | Change tracking: `changed()`, `changed_bytes()`, notifies syntax and diff. |
| `ops.c` | Operator commands: yank, delete, put, format, shift, filter, sort. |
| `register.c` | Named and unnamed registers, clipboard register bridging. |
| `undo.c` | Undo/redo tree.  Persistent undo-file support. |
| `indent.c` | Auto-indentation logic. |
| `cindent.c` | C-language indentation engine (`:set cindent`). |
| `textformat.c` | Text formatting (`:gq`), paragraph shaping. |
| `textobject.c` | Text-object selection: `aw`, `iw`, `as`, `is`, `a"`, `i"`, etc. |
| `textprop.c` | Text properties API for virtual text and diagnostic markers. |

### Normal-Mode and Command-Line

| File | Role |
|---|---|
| `normal.c` | The normal-mode command dispatcher.  Maps keystrokes to `nv_*` handler functions.  Uses `nv_cmds.h` and `nv_cmdidxs.h` index tables. |
| `ex_docmd.c` | Ex-command dispatcher.  Parses `:` commands (`:edit`, `:write`, `:quit`, …) using `ex_cmds.h` / `ex_cmdidxs.h` command tables. |
| `ex_cmds.c` | Implementation of many ex commands. |
| `ex_cmds2.c` | More ex command implementations (`:source`, `:runtime`, …). |
| `ex_getln.c` | Command-line input — editing, history, completion. |
| `ex_eval.c` | `:try`/`:catch`/`:finally`/`:throw` — exception handling. |
| `cmdexpand.c` | Command-line completion engine (Tab expansion). |
| `cmdhist.c` | Command-line history ring. |
| `usercmd.c` | User-defined `:command` handling. |

### Motion, Search, and Navigation

| File | Role |
|---|---|
| `move.c` | Cursor motion — `j`, `k`, `w`, `b`, `CTRL-D`, scrolling. |
| `search.c` | Pattern search (`/`, `?`, `n`, `N`, `:substitute`). |
| `regexp.c` | Regex dispatch: chooses BT or NFA engine. |
| `regexp_bt.c` | Backtracking regex engine. |
| `regexp_nfa.c` | NFA-based regex engine. |
| `mark.c` | Named marks, jump list, change list. |
| `tag.c` | Tags file navigation (`:tag`, `CTRL-]`). |
| `findfile.c` | `'path'`-based file search (`:find`, `gf`). |
| `fold.c` | Code folding — six methods (manual, indent, expr, syntax, diff, marker). |
| `match.c` | `:match` and `matchadd()` highlighting. |
| `fuzzy.c` | Fuzzy matching for completion. |

### Expression Evaluation (MNVscript)

| File | Role |
|---|---|
| `eval.c` | Legacy MNVscript expression parser.  Recursive-descent: `eval0()` → `eval9()`.  `num_divide()`, `num_modulus()` with safe divide-by-zero handling. |
| `evalbuffer.c` | Buffer-related evaluation functions. |
| `evalfunc.c` | Built-in function implementations (`len()`, `map()`, `filter()`, …). |
| `evalvars.c` | Variable management (`g:`, `b:`, `w:`, `t:`, `l:`, `s:`, `v:`). |
| `evalwindow.c` | Window-related evaluation functions. |
| `typval.c` | `typval_T` operations — the typed-value core of the evaluator. |
| `dict.c` | Dictionary type implementation. |
| `list.c` | List type implementation. |
| `blob.c` | Blob (byte-array) type. |
| `tuple.c` | Tuple type. |
| `float.c` | Floating-point operations. |
| `json.c` | JSON encode/decode. |
| `strings.c` | String utility functions. |
| `hashtab.c` | Hash table used for dictionaries and symbol tables. |

### MNV9 Compiler and VM

| File | Role |
|---|---|
| `mnv9script.c` | `:mnv9script`, `:import`, `:export` commands.  `in_mnv9script()` detects MNV9 mode via `SCRIPT_VERSION_MNV9`. |
| `mnv9compile.c` | Bytecode compiler: transforms MNV9 source into instruction sequences. |
| `mnv9execute.c` | Bytecode virtual machine: executes compiled MNV9 functions. |
| `mnv9expr.c` | MNV9 expression compilation — types, operators. |
| `mnv9type.c` | MNV9 type system — type checking, inference, generics resolution. |
| `mnv9instr.c` | Instruction definitions for the MNV9 VM. |
| `mnv9cmds.c` | MNV9-specific commands: `def`, `enddef`, `class`, `enum`, etc. |
| `mnv9class.c` | MNV9 class and object system — `class`, `interface`, `extends`. |
| `mnv9generics.c` | Generic type support (`<T>`, `<K, V>`, etc.). |
| `mnv9.h` | MNV9-specific type definitions and constants. |

### Syntax and Highlighting

| File | Role |
|---|---|
| `syntax.c` | Syntax highlighting engine.  `syn_pattern` struct, region matching, syncing. |
| `highlight.c` | Highlight group management, `:highlight` command, attribute resolution. |

### Display and Drawing

| File | Role |
|---|---|
| `drawline.c` | Renders a single screen line, handling line numbers, signs, folds, concealing, text properties. |
| `drawscreen.c` | Top-level screen redraw orchestration — decides which lines need updating. |
| `screen.c` | Screen buffer management — `ScreenLines[]`, `ScreenAttrs[]`, `ScreenCols[]`. |
| `ui.c` | Abstract UI layer bridging terminal / GUI drawing. |
| `term.c` | Terminal capability handling — termcap/terminfo, escape sequences. |
| `popupmenu.c` | Insert-mode completion popup menu. |
| `popupwin.c` | Floating popup windows (`:popup`, `popup_create()`). |
| `sign.c` | Sign column management. |

### File I/O

| File | Role |
|---|---|
| `fileio.c` | Reading/writing files, handling encodings, line endings, encryption. |
| `filepath.c` | File path manipulation — expansion, completion, path separators. |
| `bufwrite.c` | Buffer-write logic split from `fileio.c`. |

### Terminal Emulator

| File | Role |
|---|---|
| `terminal.c` | `:terminal` implementation.  `struct terminal_S` wraps a `VTerm` and connects it to a `job_T`.  Three parts: generic code, MS-Windows (winpty/conpty), Unix (PTY). |
| `libvterm/` | Embedded copy of libvterm — the VT100/xterm terminal emulation library. |

### Job and Channel

| File | Role |
|---|---|
| `channel.c` | Socket and pipe-based IPC.  `channel_read()`, `channel_get_mode()`, `channel_part_send()`. Supports raw, JSON, JS, and NL (newline) modes. |
| `job.c` | Process spawning and management for `job_start()`. |
| `netbeans.c` | NetBeans IDE interface protocol handler (over channels). |

### Clipboard and Selection

| File | Role |
|---|---|
| `clipboard.c` | Cross-platform clipboard — Visual selection to/from system clipboard.  Wayland clipboard integration via `wayland.h` macros.  Provider abstraction (`clip_provider_is_available()`). |
| `wayland.c` | Wayland display connection (`vwl_connection_T`), data-offer monitoring, focus-stealing clipboard.  Functions prefixed `vwl_` for abstractions, `wayland_` for global connection.  `vwl_connection_flush()`, `vwl_connection_dispatch()`. |
| `wayland.h` | Wayland types: `vwl_connection_T`, `vwl_seat_S`, `vwl_data_protocol_T` enum.  Protocol includes for ext-data-control, wlr-data-control, xdg-shell, primary-selection. |

### GUI Subsystem

| File | Role |
|---|---|
| `gui.c` | GUI core — `gui_start()`, `gui_attempt_start()`, scrollbar management, drawing dispatch.  Holds global `gui_T gui`. |
| `gui.h` | GUI macros (`TEXT_X`, `FILL_Y`, `X_2_COL`), scrollbar indices, orientation enums. |
| `gui_gtk.c` | GTK 2/3 widget creation — toolbar, dialogs, find/replace. Callbacks: `entry_activate_cb()`, `entry_changed_cb()`, `find_replace_cb()`. |
| `gui_gtk_f.c` / `gui_gtk_f.h` | GTK form widget — custom container for the editor area. |
| `gui_gtk_x11.c` | GTK+X11 integration — display init, key translation, selection, DnD. |
| `gui_motif.c` | Motif toolkit backend. |
| `gui_x11.c` | Raw X11 backend (used by Motif). |
| `gui_w32.c` | Win32 native GUI backend. |
| `gui_haiku.cc` / `gui_haiku.h` | Haiku OS GUI backend. |
| `gui_photon.c` | QNX Photon GUI backend. |
| `gui_beval.c` | Balloon-eval tooltip windows for GUIs. |
| `gui_xim.c` | X Input Method integration. |
| `gui_xmdlg.c` | Motif dialog helpers. |
| `gui_xmebw.c` / `gui_xmebw.h` / `gui_xmebwp.h` | Motif enhanced button widget. |
| `gui_dwrite.cpp` / `gui_dwrite.h` | DirectWrite text rendering (Windows). |

### Platform Abstraction

| File | Role |
|---|---|
| `os_unix.c` | Unix (and OS/2, Atari MiNT) system calls — signals, process control, terminal setup, file locking. |
| `os_unix.h` | Unix system includes and defines. |
| `os_win32.c` | Win32 system calls. |
| `os_win32.h` | Win32 includes and defines. |
| `os_mswin.c` | Shared MS-Windows functions (used by both console and GUI). |
| `os_amiga.c` / `os_amiga.h` | Amiga OS support. |
| `os_mac_conv.c` | macOS encoding conversion. |
| `os_macosx.m` | Objective-C integration for macOS (pasteboard, services). |
| `os_dos.h` | MS-DOS compatibility defines. |
| `os_haiku.h` / `os_haiku.rdef.in` | Haiku resource definitions. |
| `os_qnx.c` / `os_qnx.h` | QNX-specific code. |
| `os_vms.c` / `os_vms_conf.h` | OpenVMS support. |
| `os_w32dll.c` / `os_w32exe.c` | Win32 DLL/EXE entry points for `MNVDLL` builds. |
| `pty.c` | Pseudo-terminal allocation (Unix). |
| `iscygpty.c` / `iscygpty.h` | Cygwin PTY detection. |

### Miscellaneous

| File | Role |
|---|---|
| `misc1.c` | Miscellaneous utilities — beep, langmap, various helpers. |
| `misc2.c` | More miscellaneous — string comparison, strncpy wrappers. |
| `message.c` | User-facing messages — `msg()`, `emsg()`, `semsg()`, `iemsg()`. |
| `getchar.c` | Input character queue — typeahead, recording, mappings. |
| `mouse.c` | Mouse event handling (terminal mouse protocols). |
| `map.c` | Key mapping (`:map`, `:noremap`, etc.). |
| `menu.c` | Menu system (`:menu`, GUI menus). |
| `autocmd.c` | Autocommand infrastructure — `:autocmd`, `BufEnter`, etc. |
| `arglist.c` | Argument list management (`:args`, `:argadd`). |
| `locale.c` | Locale and language handling. |
| `mbyte.c` | Multi-byte encoding — UTF-8, locale conversion. |
| `charset.c` | Character classification — `mnv_isdigit()`, `mnv_isalpha()`, keyword chars. |
| `digraph.c` | Digraph input (`CTRL-K`). |
| `hardcopy.c` | `:hardcopy` PostScript printing. |
| `help.c` | Help system — `:help` tag lookup and display. |
| `spell.c` / `spellfile.c` / `spellsuggest.c` | Spell checker. |
| `diff.c` | Diff mode orchestration. |
| `crypt.c` / `crypt_zip.c` / `blowfish.c` / `sha256.c` | Encryption and hashing. |
| `if_cscope.c` | cscope interface. |
| `if_xcmdsrv.c` | X11 client-server (remote commands). |
| `clientserver.c` | Client-server architecture (`:remote`). |
| `scriptfile.c` | `:source` command — script loading and execution. |
| `session.c` | `:mksession` / `:mkview` — session persistence. |
| `logfile.c` | Debug logging. |
| `profiler.c` | Script and function profiling. |
| `testing.c` | Test framework functions: `assert_equal()`, `test_*()` builtins. |
| `debugger.c` | `:breakadd`, `:debug` — script debugger. |
| `time.c` | Time-related functions — reltime, timers. |
| `sound.c` | Sound playback via libcanberra (or macOS APIs). |
| `mnvinfo.c` | `.mnvinfo` file — persistent history, marks, registers. |

### Embedded Libraries

| Directory | Role |
|---|---|
| `src/libvterm/` | Terminal emulation (VT100/xterm).  Built as a static library `libvterm` linked into `mnv`. |
| `src/xdiff/` | Diff algorithms — `xdiffi.c` (Myers), `xpatience.c` (patience), `xhistogram.c` (histogram), `xprepare.c`, `xemit.c`, `xutils.c`.  Built as an `OBJECT` library. |
| `src/xxd/` | Hex dump utility.  Separate executable.  Built via `add_subdirectory(src/xxd)`. |
| `src/tee/` | Windows `tee` utility for test infrastructure. |

### Generated Files

| File | How generated |
|---|---|
| `auto/config.h` | `cmake/config.h.cmake` processed by `configure_file()` (CMake) or `configure.ac` → `config.h.in` (Autoconf). |
| `auto/pathdef.c` | `cmake/pathdef.c.cmake` processed by `configure_file()`.  Embeds install paths, compiler flags, compiled-by string. |
| `auto/osdef.h` | Stub file on modern systems; the Autoconf build generates it from `osdef.sh` + `osdef1.h.in` / `osdef2.h.in`. |
| `auto/wayland/*.c` / `auto/wayland/*.h` | Generated by `wayland-scanner` from protocol XML files. |
| `ex_cmdidxs.h` | Command index table (generated by `create_cmdidxs.mnv`). |
| `nv_cmdidxs.h` | Normal-mode command index (generated by `create_nvcmdidxs.c` / `create_nvcmdidxs.mnv`). |

---

## Key Data Structures

### `buf_T` (buffer)

Defined in `structs.h` as `typedef struct file_buffer buf_T`.  A buffer
represents an in-memory file.  Buffers form a doubly-linked list
(`b_next` / `b_prev`).  Each buffer has a `memline_T` (`b_ml`) backed by a
swap file, option values, undo tree, syntax state, and sign list.

### `win_T` (window)

Defined in `structs.h` as `typedef struct window_S win_T`.  A window is a
viewport onto a buffer.  Windows are organised in `frame_T` trees for
horizontal/vertical splits.

### `pos_T` (position)

```c
typedef struct
{
    linenr_T    lnum;     // line number
    colnr_T     col;      // column number
    colnr_T     coladd;   // extra virtual column
} pos_T;
```

### `typval_T` (typed value)

The central value type for the expression evaluator.  Discriminated union
tagged by `v_type`.  Can hold numbers, strings, floats, lists, dicts, blobs,
tuples, partial functions, jobs, channels, classes, objects, and more.

### `garray_T` (growing array)

```c
typedef struct growarray
{
    int     ga_len;         // current number of items used
    int     ga_maxlen;      // maximum number of items possible
    int     ga_itemsize;    // sizeof(item)
    int     ga_growsize;    // number of items to grow each time
    void    *ga_data;       // pointer to the first item
} garray_T;
```

Used ubiquitously — from argument lists to Wayland seat lists.

### `gui_T` (GUI state)

Global struct holding all GUI state: widget handles, fonts, colours, scrollbars,
tabline, geometry.  Declared in `gui.c`:

```c
gui_T gui;
```

### `mparm_T` (main parameters)

Struct collecting all startup parameters (argc, argv, feature flags, window
counts, edit mode, etc.) and passed through the chain of initialisation
functions in `main.c`:

```c
static mparm_T params;
```

### `vwl_connection_T` (Wayland connection)

```c
struct vwl_connection_S {
    struct { struct wl_display *proxy; int fd; } display;
    struct { struct wl_registry *proxy; } registry;
    struct {
        garray_T seats;
        struct zwlr_data_control_manager_v1 *zwlr_data_control_manager_v1;
        struct ext_data_control_manager_v1  *ext_data_control_manager_v1;
        ...
    } gobjects;
};
```

---

## Initialisation Sequence

When `main()` runs, it follows this sequence (see `src/main.c`):

1. **`mch_early_init()`** — low-level OS init (before any memory is usable).
2. **`CLEAR_FIELD(params)`** — zero the `mparm_T` struct.
3. **`autocmd_init()`** — set up autocommand tables.
4. **Interpreter init** — `mnv_ruby_init()`, `mnv_tcl_init()` if compiled in.
5. **`common_init_1()`** — first batch of shared init (allocator, hash tables,
   options, global variables).
6. **`--startuptime` / `--log` scan** — find these flags early for logging.
7. **`--clean` scan** — detect clean mode before sourcing rcfiles.
8. **`common_init_2(&params)`** — second batch (terminal setup, option defaults,
   langmap, langmenu).
9. **`command_line_scan(&params)`** — full argument parsing.
10. **`check_tty(&params)`** — verify stdin/stdout are terminals.
11. **`source_startup_scripts(&params)`** — load system and user `.mnvrc`.
12. **`create_windows(&params)`** — set up initial window layout.
13. **`exe_pre_commands(&params)`** — run `--cmd` arguments.
14. **`edit_buffers(&params, cwd)`** — load file arguments.
15. **`exe_commands(&params)`** — run `-c` arguments.
16. **`main_start_gui()`** — launch GUI event loop if applicable.
17. **Main loop** — the editor enters `normal_cmd()` dispatching in a loop.

---

## Feature Guard System

Optional features are controlled by preprocessor guards of the form `FEAT_*`.
The hierarchy is defined in `src/feature.h`:

```c
#ifdef FEAT_HUGE
# define FEAT_NORMAL
#endif
#ifdef FEAT_NORMAL
# define FEAT_TINY
#endif
```

Individual features cascade from the tier:

```c
// +folding — requires +normal
#ifdef FEAT_NORMAL
# define FEAT_FOLDING
#endif

// +langmap — requires +huge
#ifdef FEAT_HUGE
# define FEAT_LANGMAP
#endif
```

The CMake build sets the top-level feature in `auto/config.h` via:

```cmake
set(MNV_FEATURE "huge" CACHE STRING "Feature level: tiny, normal, huge")
```

And the `feature.h` cascade does the rest.

---

## Threading Model

MNV is fundamentally **single-threaded**.  The main loop processes one
keystroke at a time.  Asynchronous I/O is handled through `select()`/`poll()`
multiplexing in the channel layer (`channel.c`).  GUI event loops (GTK, Win32)
integrate with MNV's own event processing.

The only use of pthreads is optional and runtime-specific (e.g., timer signals
on some platforms).  The Wayland code uses the display file descriptor with
`poll()` or `select()` for non-blocking dispatch.

---

## Test Architecture

### Unit Tests

Four dedicated unit-test executables are built by the CMake function
`mnv_add_unit_test()`:

| Test | Replaces | Tests |
|---|---|---|
| `json_test` | `json.c` | JSON encode/decode |
| `kword_test` | `charset.c` | Keyword character classification |
| `memfile_test` | `memfile.c` | Memory file operations |
| `message_test` | `message.c` | Message formatting |

Each test replaces `main.c` and one other source with a test variant that
provides its own `main()`.

### Script Tests

The bulk of the test suite lives in `src/testdir/`.  Tests are `.mnv` scripts
run by the `mnv` binary itself.  Categories:

- **nongui** — all terminal-mode tests.
- **indent** — indentation rule tests under `runtime/indent/`.
- **syntax** — syntax highlighting tests under `runtime/syntax/`.

Tests are invoked via `ctest` or `make test` and have generous timeouts
(3600 s for script tests).

---

## Build System Duality

MNV maintains **two** build systems:

1. **CMake** (`CMakeLists.txt`) — the primary, modern build.  Handles
   dependency detection, feature configuration, and test registration.  Uses
   `CMakePresets.json` for named configurations.

2. **Autoconf + Make** (`src/configure.ac`, `src/Makefile`, top-level
   `Makefile`, plus platform-specific `Make_*.mak` files).  The legacy build
   that works on the widest range of systems.

Both produce the same `mnv` binary; features and dependencies are detected
independently by each system.

---

*This document describes MNV 10.0 as of build 287 (2026-04-03).*
