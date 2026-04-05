# MNV — Overview

## What Is MNV?

MNV (recursive acronym: **MNV is not Vim**) is a highly capable, open-source
text editor descended from the classic UNIX editor Vi.  It is developed under
the Project Tick umbrella and ships as the `mnv` binary.  The current major
release is **MNV 10.0** (version string `MNV_VERSION_MAJOR 10`,
`MNV_VERSION_MINOR 0`), with build numbers tracked in
`src/version.h`:

```c
#define MNV_VERSION_MAJOR   10
#define MNV_VERSION_MINOR   0
#define MNV_VERSION_BUILD   287
```

MNV maintains near-complete compatibility with Vi while adding a vast array of
modern editing features.  It targets the same niche as its ancestors — fast,
keyboard-driven text editing for programmers and system administrators — but
extends the experience with a graphical user interface, an embedded scripting
language, asynchronous job control, a built-in terminal emulator, Wayland
clipboard integration, and much more.

---

## Project Identity

| Field | Value |
|---|---|
| Full name | MNV — MNV is not Vim |
| Repository | `Project-Tick/Project-Tick` (under `mnv/`) |
| License | See `COPYING.md` and `LICENSE` in the project root |
| Language | C (C99), with MNV9 script for runtime |
| Build systems | CMake (primary), GNU Autoconf + Make (legacy) |
| Version macro | `MNV_VERSION_LONG` → `"MNV - MNV is not Vim 10.0 (2026 Apr 3)"` |

The project description in `CMakeLists.txt` reads:

```cmake
project(MNV
    DESCRIPTION "MNV - MNV is not Vim"
    LANGUAGES C
)
```

---

## Design Philosophy

1. **Vi compatibility first.**  Users who have Vi "in the fingers" can work
   immediately.  Every normal-mode, insert-mode, and command-line keystroke
   from Vi works identically unless a feature consciously extends it.

2. **Layered feature sets.**  The build system exposes three feature tiers
   defined in `src/feature.h`:

   ```c
   // +tiny    — no optional features enabled, not even +eval
   // +normal  — a default selection of features enabled
   // +huge    — all possible features enabled.
   ```

   Each tier is a strict superset of the previous one:

   ```c
   #ifdef FEAT_HUGE
   # define FEAT_NORMAL
   #endif
   #ifdef FEAT_NORMAL
   # define FEAT_TINY
   #endif
   ```

   On Unix, macOS and Windows the default is `+huge`.

3. **Portability.**  MNV builds on Linux, macOS, Windows (7 – 11), Haiku, VMS,
   and nearly every UNIX variant.  Platform-specific code lives in dedicated
   `os_*.c` files (`os_unix.c`, `os_win32.c`, `os_amiga.c`, `os_mac_conv.c`,
   etc.), keeping the core editor portable.

4. **Keyboard efficiency.**  All commands use normal keyboard characters.
   Function keys and mouse are optionally available but never required.

---

## Feature Highlights

### Multi-level Undo / Redo

MNV records every editing operation in an undo tree (`src/undo.c`).  Users can
walk the tree with `u`, `CTRL-R`, and the `:undolist` / `:undo` commands.  The
undo file is persisted across sessions when `'undofile'` is set.

### Syntax Highlighting

Implemented in `src/syntax.c` (guarded by `FEAT_SYN_HL`), MNV ships hundreds
of syntax definitions under `runtime/syntax/`.  The `syn_pattern` struct drives
the highlighting engine:

```c
typedef struct syn_pattern
{
    char    sp_type;
    char    sp_syncing;
    short   sp_syn_match_id;
    short   sp_off_flags;
    int     sp_offsets[SPO_COUNT];
    int     sp_flags;
    int     sp_ic;
    ...
} syn_pattern;
```

### Built-in Terminal Emulator

When compiled with `FEAT_TERMINAL`, MNV embeds a terminal emulator
(`src/terminal.c`) backed by **libvterm** (`src/libvterm/`).  A terminal buffer
is opened with `:terminal` and connected to a background job via the
channel/job infrastructure.

```c
struct terminal_S {
    term_T    *tl_next;
    VTerm     *tl_vterm;
    job_T     *tl_job;
    buf_T     *tl_buffer;
    ...
};
```

### Asynchronous Jobs and Channels

`src/channel.c` and `src/job.c` provide the `+channel` / `+job` features.
Channels communicate over sockets (TCP, Unix domain), pipes, or PTYs. This
powers the terminal emulator, the NetBeans interface, Language Server
connections, and user scripts.

### MNV9 Script

MNV ships a modernized scripting dialect called **MNV9 script**
(`src/mnv9script.c`, `src/mnv9compile.c`, `src/mnv9execute.c`,
`src/mnv9expr.c`, `src/mnv9type.c`, `src/mnv9instr.c`, `src/mnv9cmds.c`,
`src/mnv9class.c`, `src/mnv9generics.c`).  Detection of MNV9 mode happens at
runtime:

```c
int
in_mnv9script(void)
{
    return (current_sctx.sc_version == SCRIPT_VERSION_MNV9
                     || (cmdmod.cmod_flags & CMOD_MNV9CMD))
                && !(cmdmod.cmod_flags & CMOD_LEGACY);
}
```

MNV9 introduces strict typing, classes, generics, compiled-to-bytecode
execution, and `import` / `export` semantics.

### Graphical User Interface

MNV supports multiple GUI toolkits (GTK 2, GTK 3, Motif, Win32, Haiku, Photon)
through a clean backend abstraction in `src/gui.c` / `src/gui.h`.  The global
`gui_T gui` struct holds all GUI state.  Platform backends live in
`gui_gtk.c`, `gui_gtk_x11.c`, `gui_motif.c`, `gui_w32.c`, `gui_haiku.cc`, etc.

### Wayland Clipboard

Native Wayland clipboard support is implemented in `src/wayland.c` and
`src/wayland.h` (guarded by `FEAT_WAYLAND`).  It uses the
`ext-data-control-v1`, `wlr-data-control-unstable-v1`, and optionally the core
`wl_data_device_manager` protocols.  The clipboard code in `src/clipboard.c`
dispatches through protocol-agnostic macros defined at the end of `wayland.c`.

```c
vwl_connection_T    *wayland_ct;
```

### Encryption

MNV supports multiple encryption methods.  Blowfish is implemented in
`src/blowfish.c`, ZIP-based crypt in `src/crypt_zip.c`, and the modern
`xchacha20` method uses **libsodium** when `HAVE_SODIUM` is defined.

### Regular Expressions

Two regex engines coexist in `src/regexp.c`:

- **BT engine** (`src/regexp_bt.c`) — backtracking, traditional.
- **NFA engine** (`src/regexp_nfa.c`) — NFA-based, faster for many patterns.

The dispatcher chooses automatically or can be forced via `'regexpengine'`.

### Quickfix / Location Lists

`src/quickfix.c` implements the `:make`, `:grep`, `:copen`, `:lopen` family of
commands for compiler-output navigation.

### Spell Checking

`src/spell.c`, `src/spellfile.c`, `src/spellsuggest.c` provide the `+spell`
feature with support for word lists, affixes, compound words, and suggestions.

### Diff Mode

`src/diff.c` together with the embedded `src/xdiff/` library (xdiffi,
xpatience, xhistogram algorithms) delivers side-by-side diff viewing.

### Folding

`src/fold.c` drives code folding — manual, indent, expr, syntax, diff, and
marker methods.

### Text Properties / Virtual Text

`src/textprop.c` provides the text-property API used by plugins for inline
virtual text, diagnostics markers, and similar overlays.

### Popup Windows

`src/popupwin.c` and `src/popupmenu.c` implement floating popup windows and
the insert-mode completion menu.

---

## Runtime Files

The `runtime/` directory is installed alongside the binary and contains:

| Directory | Purpose |
|---|---|
| `runtime/doc/` | Help files (`:help`) |
| `runtime/syntax/` | Syntax highlighting definitions |
| `runtime/ftplugin/` | File-type plugins |
| `runtime/indent/` | Indentation rules |
| `runtime/colors/` | Color schemes |
| `runtime/compiler/` | Compiler integration |
| `runtime/autoload/` | Autoloaded script functions |
| `runtime/plugin/` | Global plugins |
| `runtime/pack/` | Package directory |
| `runtime/tutor/` | The `mnvtutor` training material |
| `runtime/keymap/` | Keyboard mappings for non-Latin scripts |
| `runtime/import/` | MNV9 import modules |
| `runtime/spell/` | Spell-check word-list files |
| `runtime/print/` | PostScript printing support |
| `runtime/lang/` | UI translation message files |
| `runtime/macros/` | Example macros |
| `runtime/tools/` | Auxiliary tools |

Essential runtime scripts loaded at startup:

- `defaults.mnv` — sensible defaults for new users.
- `filetype.mnv` / `ftoff.mnv` — filetype detection on/off.
- `ftplugin.mnv` / `ftplugof.mnv` — filetype plugins on/off.
- `indent.mnv` / `indoff.mnv` — filetype indentation on/off.
- `menu.mnv` / `synmenu.mnv` — GUI menu definitions.
- `scripts.mnv` — fallback filetype detection by content.
- `optwin.mnv` — the `:options` window.
- `mswin.mnv` — Windows-style key bindings.

---

## Executable Variants

A single `mnv` binary behaves differently depending on the name it is invoked
with.  The CMake install step creates symlinks:

| Symlink | Behaviour |
|---|---|
| `mnv` | Normal mode |
| `ex` | Start in Ex mode (`:` prompt) |
| `view` | Read-only mode (`-R`) |
| `rmnv` | Restricted mode |
| `rview` | Restricted + read-only |
| `mnvdiff` | Start in diff mode |
| `vi` | Compatibility alias |
| `vim` | Compatibility alias |
| `gmnv` | GUI mode (when GUI is compiled in) |
| `gview` | GUI + read-only |
| `gmnvdiff` | GUI + diff mode |
| `rgmnv` | GUI + restricted |
| `rgview` | GUI + restricted + read-only |
| `emnv` | "Easy mode" GUI |
| `eview` | "Easy mode" GUI + read-only |
| `gvi` / `gvim` | GUI compatibility aliases |

---

## The Tutor

MNV bundles a one-hour interactive tutorial.  It is typically started with:

```sh
mnvtutor
```

The tutor files reside in `runtime/tutor/` and the launcher scripts are
`mnvtutor.com` (VMS) and `mnvtutor.bat` (Windows) at the project root, plus
`src/mnvtutor` / `src/gmnvtutor` for Unix.

---

## Auxiliary Tool: xxd

The `src/xxd/` directory contains **xxd**, a hex-dump / reverse-hex-dump
utility.  It is built as a separate executable by the CMake build
(`add_subdirectory(src/xxd)`).

---

## Relation to Vim and Vi

MNV is a fork that diverges from upstream Vim by:

- Renaming the project and binary to `mnv`.
- Adopting a CMake-first build system alongside the legacy Autoconf build.
- Adding first-class Wayland clipboard support (`FEAT_WAYLAND`,
  `FEAT_WAYLAND_CLIPBOARD`).
- Using `mnv9script` naming for the modern scripting dialect.
- Storing runtime files in `mnv`-prefixed paths.
- Maintaining the project under the Project Tick organisation.

Despite these changes, MNV intentionally preserves Vi and Vim compatibility so
that existing workflows, plugins, and muscle memory carry over unchanged.

---

## CI and Quality Assurance

The project uses:

- **GitHub Actions** — primary CI (linux, macOS, coverage).
- **Appveyor** — Windows CI.
- **Cirrus CI** — FreeBSD builds.
- **Codecov** — coverage tracking.
- **Coverity Scan** — static analysis.
- **Fossies codespell** — spell-checking source comments.

Unit tests (`json_test.c`, `kword_test.c`, `memfile_test.c`, `message_test.c`)
validate isolated subsystems.  The full test suite lives in `src/testdir/` and
is driven by `make test` or `ctest`.

---

## Further Reading

| Resource | Location |
|---|---|
| Build instructions | `src/INSTALL`, the `building.md` handbook page |
| Architecture | The `architecture.md` handbook page |
| GUI details | The `gui-extension.md` handbook page |
| Command-line usage | The `scripting.md` handbook page |
| Platform notes | The `platform-support.md` handbook page |
| Coding conventions | The `code-style.md` handbook page |
| Contributing guide | `CONTRIBUTING.md` in the project root |
| MNV9 scripting | `README_MNV9.md` |
| Help system | `:help` inside MNV, or `runtime/doc/help.txt` |

---

## Glossary

| Term | Meaning |
|---|---|
| `buf_T` | The C struct representing a buffer (in-memory file). |
| `win_T` | A window — a viewport onto a buffer. |
| `pos_T` | A cursor position: `{lnum, col, coladd}`. |
| `typval_T` | A typed value in the expression evaluator. |
| `garray_T` | A generic growable array used throughout the codebase. |
| `mparm_T` | The struct holding `main()` parameters passed between init functions. |
| `gui_T` | Global GUI state. |
| `term_T` | A terminal emulator instance. |
| Feature guard | A `#ifdef FEAT_*` preprocessor conditional controlling optional code. |
| MNV9 | The modern, statically-typed scripting dialect. |
| libvterm | The embedded terminal emulation library. |
| xdiff | The embedded diff library (xdiffi, xhistogram, xpatience). |
| xxd | The bundled hex-dump utility. |

---

*This document describes MNV 10.0 as of build 287 (2026-04-03).*
