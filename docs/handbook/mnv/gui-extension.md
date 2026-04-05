# MNV — GUI Extension

## Overview

MNV supports a **graphical user interface (GUI)** through a toolkit-agnostic
abstraction layer.  The GUI is optional — MNV works perfectly as a terminal
application — but when enabled it adds menus, toolbars, scrollbars, tearoff
menus, tablines, font selection, direct mouse integration, drag-and-drop, and
balloon-eval tooltips.

The GUI is activated by:
- Invoking the binary as `gmnv` (via symlink).
- Running `:gui` from within terminal MNV.
- Compiling with `FEAT_GUI` and having no `--nofork` / `-f` flag.

The compile-time guard for all GUI code is:

```c
#if defined(FEAT_GUI_MOTIF) \
    || defined(FEAT_GUI_GTK) \
    || defined(FEAT_GUI_HAIKU) \
    || defined(FEAT_GUI_MSWIN) \
    || defined(FEAT_GUI_PHOTON)
# if !defined(FEAT_GUI) && !defined(NO_X11_INCLUDES)
#  define FEAT_GUI
# endif
#endif
```

---

## Architecture

### Abstraction Layer: `gui.c` / `gui.h`

The file `src/gui.c` is the **core GUI dispatcher**.  It owns the global
`gui_T gui` struct and provides toolkit-independent functions that delegate to
`gui_mch_*()` ("machine-specific") callbacks implemented in each backend.

```c
// src/gui.c
gui_T gui;
```

Key functions in `gui.c`:

| Function | Purpose |
|---|---|
| `gui_start()` | Entry point — init toolkit, fork if needed, start event loop. |
| `gui_attempt_start()` | Try to initialise the GUI; fall back to terminal on failure. |
| `gui_do_fork()` | Fork the process so `gmnv file` detaches from the shell. |
| `gui_read_child_pipe()` | IPC between parent and GUI child after fork. |
| `gui_check_pos()` | Clamp cursor within drawable area. |
| `gui_reset_scroll_region()` | Reset the scrollable region to full screen. |
| `gui_outstr()` | Output a string to the GUI display. |
| `gui_screenchar()` | Draw a single character at a screen position. |
| `gui_outstr_nowrap()` | Draw a string without line wrapping. |
| `gui_delete_lines()` / `gui_insert_lines()` | Scroll line ranges. |
| `gui_xy2colrow()` | Convert pixel coordinates to character row/column. |
| `gui_do_scrollbar()` | Enable/disable a scrollbar for a window. |
| `gui_update_horiz_scrollbar()` | Refresh horizontal scrollbar state. |
| `gui_set_fg_color()` / `gui_set_bg_color()` | Set foreground/background. |
| `init_gui_options()` | Initialise GUI-related option defaults. |
| `xy2win()` | Find which window a pixel coordinate falls in. |

### The `gui_T` Data Structure

Declared in `src/gui.h`, `gui_T` holds all mutable GUI state.  Its fields
include (representative, not exhaustive):

- Widget/window handles (toolkit-specific, cast to `void *` or typed per
  backend).
- `gui.in_use` — boolean, TRUE when GUI is active.
- `gui.starting` — TRUE during initialisation.
- `gui.dofork` — whether to fork on startup.
- `gui.char_width` / `gui.char_height` / `gui.char_ascent` — font metrics.
- `gui.border_offset` — pixel offset for the text area border.
- `gui.num_rows` / `gui.num_cols` — grid dimensions.
- Scrollbar state arrays.
- Colour values for foreground, background, scrollbar, menu.
- Tabline widget handles (for `FEAT_GUI_TABLINE`).

### Coordinate Macros

`gui.h` defines macros for converting between character cells and pixel
coordinates:

```c
// Non-MSWIN (X11/GTK/Motif/Haiku/Photon):
#define TEXT_X(col)   ((col) * gui.char_width  + gui.border_offset)
#define TEXT_Y(row)   ((row) * gui.char_height + gui.char_ascent + gui.border_offset)
#define FILL_X(col)   ((col) * gui.char_width  + gui.border_offset)
#define FILL_Y(row)   ((row) * gui.char_height + gui.border_offset)
#define X_2_COL(x)    (((x) - gui.border_offset) / gui.char_width)
#define Y_2_ROW(y)    (((y) - gui.border_offset) / gui.char_height)

// MSWIN:
#define TEXT_X(col)   ((col) * gui.char_width)
#define TEXT_Y(row)   ((row) * gui.char_height + gui.char_ascent)
// etc.
```

### Scrollbar Constants

```c
#define SBAR_NONE       (-1)
#define SBAR_LEFT       0
#define SBAR_RIGHT      1
#define SBAR_BOTTOM     2
```

---

## GUI Backends

### GTK 2 / GTK 3  (`gui_gtk.c`, `gui_gtk_f.c`, `gui_gtk_x11.c`)

The GTK backend is the most actively maintained Linux GUI.  It consists of
three files:

**`gui_gtk.c`** — High-level GTK widget management:

- Toolbar creation and tearoff support.
- Find/Replace dialog (`find_replace_cb()`).
- Dialog entry callbacks: `entry_activate_cb()`, `entry_changed_cb()`.

It includes GTK headers conditionally:

```c
#ifdef FEAT_GUI_GTK
# if GTK_CHECK_VERSION(3,0,0)
#  include <gdk/gdkkeysyms-compat.h>
# else
#  include <gdk/gdkkeysyms.h>
# endif
# include <gdk/gdk.h>
# include <gtk/gtk.h>
#endif
```

**`gui_gtk_f.c` / `gui_gtk_f.h`** — A custom GTK container widget (the "form
widget") that manages the drawing area, scrollbars, and toolbar layout.  This
replaces GTK's standard layout containers with one optimised for MNV's needs.

**`gui_gtk_x11.c`** — Low-level integration with X11 under GTK:

- Display connection and window management.
- Keyboard input translation (GDK key events → MNV key codes).
- X selection handling (clipboard).
- Drag-and-drop (`FEAT_DND`).
- Input method support via `gui_xim.c`.

**CMake source list for GTK:**

```cmake
set(GUI_SRC
    gui.c
    gui_gtk.c
    gui_gtk_f.c
    gui_gtk_x11.c
    gui_beval.c
)
```

**GTK 3 vs GTK 2 detection:**

```cmake
if(MNV_GUI STREQUAL "auto" OR MNV_GUI STREQUAL "gtk3")
    pkg_check_modules(GTK3 QUIET gtk+-3.0)
    if(GTK3_FOUND)
        set(USE_GTK3 1)
        set(FEAT_GUI_GTK 1)
        ...
    endif()
endif()
```

GTK 3 support was added by Kazunobu Kuriyama (2016) and is now the default.

### Motif (`gui_motif.c`, `gui_x11.c`, `gui_xmdlg.c`, `gui_xmebw.c`)

The Motif backend uses the Xt/Motif widget set:

```cmake
set(GUI_SRC
    gui.c
    gui_motif.c
    gui_x11.c
    gui_beval.c
    gui_xmdlg.c
    gui_xmebw.c
)
```

- `gui_motif.c` — Motif menus, toolbar, scrollbars.
- `gui_x11.c` — Raw X11 drawing, event loop, selection.
- `gui_xmdlg.c` — Motif dialogs (file selection, font picker).
- `gui_xmebw.c` / `gui_xmebw.h` / `gui_xmebwp.h` — "Enhanced Button
  Widget" — a custom Motif widget for toolbar buttons with icons.

### Win32 (`gui_w32.c`)

The native Windows GUI uses the Win32 API directly (no toolkit):

- `gui_w32.c` — window creation, message loop, menus, scrollbars, Direct2D
  text rendering.
- `gui_dwrite.cpp` / `gui_dwrite.h` — DirectWrite rendering for high-quality
  font display on Windows (controlled by `FEAT_DIRECTX` / `FEAT_RENDER_OPTIONS`).
- `gui_w32_rc.h` — resource header for the Windows resource file (`mnv.rc`).

### Haiku (`gui_haiku.cc`, `gui_haiku.h`)

BeOS/Haiku GUI backend using the native BApplication/BWindow/BView API:

```c
#ifdef FEAT_GUI_HAIKU
# include "gui_haiku.h"
#endif
```

Supports drag-and-drop (`HAVE_DROP_FILE`).

### Photon (`gui_photon.c`)

QNX Photon microGUI backend.  Legacy, for QNX RTOS systems:

```c
#ifdef FEAT_GUI_PHOTON
# include <Ph.h>
# include <Pt.h>
# include "photon/PxProto.h"
#endif
```

---

## GUI Features

### On-the-Fly Scrolling

GTK and Win32 support immediate scroll redraw rather than deferring to the
main loop:

```c
#if defined(FEAT_GUI_MSWIN) || defined(FEAT_GUI_GTK)
# define USE_ON_FLY_SCROLL
#endif
```

### Drag and Drop

File dropping is enabled for GTK (with `FEAT_DND`), Win32, and Haiku:

```c
#if (defined(FEAT_DND) && defined(FEAT_GUI_GTK)) \
        || defined(FEAT_GUI_MSWIN) \
        || defined(FEAT_GUI_HAIKU)
# define HAVE_DROP_FILE
#endif
```

### Balloon Evaluation (Tooltips)

`gui_beval.c` implements balloon-eval — hover tooltips used for debugger
variable inspection, function signatures, and similar features.  Controlled
by `FEAT_BEVAL` / `FEAT_BEVAL_TIP`.

### Tab Page Line

When `FEAT_GUI_TABLINE` is defined, the GUI displays a tab bar at the top of
the window for switching between tab pages.

```c
#if defined(FEAT_GUI_TABLINE)
static int gui_has_tabline(void);
#endif
```

### X Input Method (XIM)

`gui_xim.c` integrates X Input Methods for composing complex characters
(CJK, etc.) on X11.  Controlled by `FEAT_XIM`:

```c
#ifdef FEAT_XIM
# ifdef FEAT_GUI_GTK
   // GTK handles XIM through GtkIMContext
# else
   // Direct XIM protocol for Motif/X11
# endif
#endif
```

### GUI Forking

On Unix when `gmnv` starts, it forks so the parent shell returns to the
prompt while the child continues as the editor.  This happens in
`gui_do_fork()`:

```c
#ifdef GUI_MAY_FORK
static void gui_do_fork(void);

static int gui_read_child_pipe(int fd);

enum {
    GUI_CHILD_IO_ERROR,
    GUI_CHILD_OK,
    GUI_CHILD_FAILED
};
#endif
```

The fork is skipped when:
- `-f` flag is given.
- `'f'` is in `'guioptions'` (`p_go`).
- A background job is running (`job_any_running()`).

### Menus

Menu definitions are loaded from `runtime/menu.mnv` and `runtime/synmenu.mnv`.
The `:menu` command adds items to the menu bar.  GUI backends render menus
using platform-native widgets.

### Fonts

GUI font handling is integrated with the `'guifont'` and `'guifontwide'`
options.  Character metrics stored in `gui.char_width`, `gui.char_height`, and
`gui.char_ascent` are critical for all coordinate conversions.

---

## CMake GUI Detection

The CMake build tries toolkits in order:

```
1. GTK 3  (pkg-config: gtk+-3.0)
2. GTK 2  (pkg-config: gtk+-2.0)
3. Motif  (FindMotif)
4. none
```

If `MNV_GUI` is set to a specific toolkit and it's not found, the build fails:

```cmake
if(NOT _gui_found)
    if(MNV_GUI STREQUAL "none" OR MNV_GUI STREQUAL "auto")
        message(STATUS "GUI: disabled")
    else()
        message(FATAL_ERROR "Requested GUI '${MNV_GUI}' not found")
    endif()
endif()
```

---

## GUI Symlinks

When the GUI is compiled in, the install step creates additional symlinks:

```cmake
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv ${_bindir}/gmnv)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv ${_bindir}/gview)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv ${_bindir}/gmnvdiff)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv ${_bindir}/rgmnv)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv ${_bindir}/rgview)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv ${_bindir}/emnv)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv ${_bindir}/eview)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv ${_bindir}/gvi)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv ${_bindir}/gvim)
```

Desktop files and icons are also installed:

```cmake
install(FILES runtime/mnv.desktop  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES runtime/gmnv.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
```

Icons at 16×16, 32×32, 48×48, and 128×128 plus a scalable SVG are placed in
the hicolor icon theme.

---

## Runtime Configuration

GUI behaviour is customised via options in `.mnvrc` or `.gmnvrc`:

| Option | Purpose |
|---|---|
| `'guifont'` | Font face and size |
| `'guifontwide'` | Font for double-width characters |
| `'guioptions'` | Flags controlling which GUI elements are shown |
| `'guicursor'` | Cursor shape in different modes |
| `'guitablabel'` | Tab page label format |
| `'guitabtooltip'` | Tab page tooltip format |
| `'linespace'` | Extra pixels between lines |
| `'columns'` / `'lines'` | Window dimensions |
| `'toolbar'` | Toolbar display flags |

The `'guioptions'` string (aliased `p_go` in `option.h`) controls flags
like `f` (foreground — don't fork), `m` (menu bar), `T` (toolbar), `r`/`l`
(scrollbars), etc.

The `runtime/gmnvrc_example.mnv` file provides a starting template.

---

*This document describes MNV 10.0 as of build 287 (2026-04-03).*
