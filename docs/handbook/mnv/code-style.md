# MNV — Code Style

## Overview

MNV follows a distinctive coding style inherited from the Vi/Vim tradition.
The style is partially documented in `.clang-format` and `.editorconfig` at
the project root, and largely defined by example in the existing codebase.

---

## Indentation and Whitespace

### `.editorconfig` rules

```ini
[*]
indent_style = tab
tab_width = 8
trim_trailing_whitespace = true
insert_final_newline = true

[*.{c,h,proto}]
indent_size = 4

[*.{md,yml,sh,bat}]
indent_style = space
indent_size = 2

[*.mnv]
indent_style = space
indent_size = 2
```

Key takeaways:

- **C/H files use hard tabs** with `tabstop=8`, `shiftwidth=4`.
- **Script files (`.mnv`) use spaces** with indent 2.
- **Markdown, YAML, shell scripts use spaces** with indent 2.

This matches the modeline at the top of every source file:

```c
/* vi:set ts=8 sts=4 sw=4 noet: */
```

Meaning:
- `ts=8` — tab stop at 8 columns.
- `sts=4` — soft tab stop, making tabs appear 4 columns wide for editing.
- `sw=4` — shift width is 4.
- `noet` — do NOT expand tabs to spaces.

### `.clang-format` configuration

The `.clang-format` file specifies detailed formatting rules:

```yaml
Language: Cpp
IndentWidth: 8
TabWidth: 8
UseTab: Always
ColumnLimit: 0
BreakBeforeBraces: Allman
AlignAfterOpenBracket: Align
AlignConsecutiveAssignments: false
AlignConsecutiveDeclarations: false
AlignEscapedNewlines: DontAlign
AlignOperands: Align
```

Note: `clang-format` is available as a reference but is **not universally
applied** — the codebase still relies heavily on manual formatting.

---

## File Headers

Every `.c` and `.h` file begins with:

```c
/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * MNV - MNV is not Vim	by Bram Moolenaar
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 * See README.txt for an overview of the MNV source code.
 */
```

The first line is a **modeline** that configures the editor for the correct
whitespace settings.  Every source file must have this.

---

## Function Definition Style

MNV uses a distinctive **K&R-like but separated** function definition style.
The return type and qualifiers go on the line **above** the function name,
which starts at column 4 (one tab indent):

```c
    static void
gui_check_pos(void)
{
    ...
}
```

```c
    int
vwl_connection_flush(vwl_connection_T *self)
{
    ...
}
```

Public functions similarly:

```c
    void
gui_start(char_u *arg UNUSED)
{
    ...
}
```

Rules:
- Return type and storage class on its own line, indented by one tab (4 visual
  columns).
- Function name starts at column 0.
- Opening brace on its own line at column 0 (Allman style).
- Parameter list may wrap with alignment.

### Static function declarations

Forward declarations follow the same pattern:

```c
static void gui_check_pos(void);
static void gui_reset_scroll_region(void);
static int gui_screenchar(int off, int flags, guicolor_T fg, guicolor_T bg, int back);
```

---

## Variable Declarations

Local variables are declared at the top of a function, before any statements.
This is required for C89 compatibility (though MNV uses C99):

```c
    void
gui_start(char_u *arg UNUSED)
{
    char_u	*old_term;
#ifdef GUI_MAY_FORK
    static int	recursive = 0;
#endif

    old_term = mnv_strsave(T_NAME);
    ...
}
```

Alignment of variable names to the same column (using tabs) is common:

```c
    int		ret;
    char_u	*buf;
    long	lnum;
    pos_T	pos;
```

---

## Comments

### Block comments

Use `/* ... */` style.  Never `//` for multi-line comments:

```c
/*
 * This is a block comment explaining the next function.
 * Multiple lines follow the same pattern.
 */
```

### Inline comments

Single-line comments use `//`:

```c
static int has_dash_c_arg = FALSE; // whether -c was given
```

This is a newer convention; older code uses `/* */` even for inline comments.

### Section headers

Major sections within a file are marked with comment banners:

```c
/*
 * Different types of error messages.
 */
```

In the CMake file, section headers use hash-line banners:

```cmake
###############################################################################
# Build options
###############################################################################
```

---

## Naming Conventions

### Functions

- Core functions: `lowercase_with_underscores` — e.g., `gui_start()`,
  `command_line_scan()`, `enter_buffer()`.
- Machine-specific functions: `mch_` prefix — e.g., `mch_early_init()`,
  `mch_exit()`.
- GUI backend functions: `gui_mch_` prefix — e.g., `gui_mch_flush()`.
- Wayland functions: `vwl_` for abstractions, `wayland_` for global
  connection — e.g., `vwl_connection_flush()`, `vwl_connection_dispatch()`.
- MNV9 functions: `mnv9_` prefix — e.g., `in_mnv9script()`.
- Test functions: named after what they test, e.g., `json_test`, `kword_test`.

### Types

- Struct typedefs end with `_T`: `buf_T`, `win_T`, `pos_T`, `typval_T`,
  `garray_T`, `gui_T`, `mparm_T`, `cellattr_T`, `vwl_connection_T`.
- Internal struct tags use `_S` suffix: `struct vwl_seat_S`,
  `struct vwl_connection_S`, `struct terminal_S`.
- Enum values: `UPPERCASE_SNAKE_CASE` — e.g., `VWL_DATA_PROTOCOL_NONE`,
  `EDIT_FILE`, `ME_UNKNOWN_OPTION`.

### Macros

- All uppercase: `FEAT_GUI`, `HAVE_CONFIG_H`, `UNUSED`, `EXTERN`, `INIT()`.
- Feature guards: `FEAT_` prefix (`FEAT_TERMINAL`, `FEAT_EVAL`,
  `FEAT_WAYLAND`).
- Detection results: `HAVE_` prefix (`HAVE_STDINT_H`, `HAVE_SELECT`,
  `HAVE_DLOPEN`).
- Dynamic library names: `DYNAMIC_` prefix (`DYNAMIC_LUA_DLL`).

### Global variables

Declared in `globals.h` with the `EXTERN` macro:

```c
EXTERN long	Rows;
EXTERN long	Columns INIT(= 80);
EXTERN schar_T	*ScreenLines INIT(= NULL);
```

The `INIT(x)` macro expands to `= x` in `main.c` and to nothing elsewhere.

---

## Preprocessor Conventions

### Feature guards

MNV uses `#ifdef FEAT_*` extensively.  Feature code is wrapped tightly:

```c
#ifdef FEAT_FOLDING
EXTERN foldinfo_T win_foldinfo;
#endif
```

Functions that only exist with certain features use `#ifdef` blocks:

```c
#if defined(FEAT_GUI_TABLINE)
static int gui_has_tabline(void);
#endif
```

### Platform guards

```c
#ifdef MSWIN
    // Windows-specific code
#endif

#ifdef UNIX
    // Unix-specific code
#endif

#if defined(MACOS_X)
    // macOS
#endif
```

### N_() for translatable strings

User-visible strings are wrapped in `N_()` for gettext extraction:

```c
N_("Unknown option argument"),
N_("Too many edit arguments"),
```

---

## Typedef Conventions

Commonly used types:

```c
typedef unsigned char	char_u;     // unsigned character
typedef signed char	int8_T;     // signed 8-bit
typedef double		float_T;    // floating point
typedef long		linenr_T;   // line number
typedef int		colnr_T;    // column number
typedef unsigned short	short_u;    // unsigned short
```

The custom `char_u` type is used instead of `char` throughout the codebase to
avoid signed-char bugs.

---

## Error Handling Patterns

### Error message functions

| Function | Use |
|---|---|
| `emsg()` | Display error message |
| `semsg()` | Formatted error (like `sprintf` + `emsg`) |
| `iemsg()` | Internal error (bug in MNV) |
| `msg()` | Informational message |
| `smsg()` | Formatted informational message |

### Return conventions

- Many functions return `OK` / `FAIL` (defined as `1` / `0`).
- Pointer-returning functions return `NULL` on failure.
- Boolean functions return `TRUE` / `FALSE`.

---

## MNV9 Script Style

For `.mnv` files (MNV9 syntax):

- Indent with 2 spaces (per `.editorconfig`).
- Use `def`/`enddef` instead of `function`/`endfunction`.
- Type annotations: `var name: type = value`.
- Use `#` for comments (not `"`).

---

## Guard Macros in Headers

Headers use traditional include guards:

```c
#ifndef MNV__H
#define MNV__H
...
#endif
```

```c
#ifndef _OPTION_H_
#define _OPTION_H_
...
#endif
```

---

## Test Code Style

Test files (`*_test.c`) follow the same style as production code.  Tests in
`src/testdir/` are MNVscript files following the `.mnv` indent style (2 spaces).

The CI enforces code style via `test_codestyle.mnv` — contributions must pass
this check.

---

## Summary of Key Rules

| Aspect | Rule |
|---|---|
| Tabs vs spaces in C | Hard tabs, `tabstop=8`, `shiftwidth=4` |
| Tabs vs spaces in .mnv | Spaces, indent 2 |
| Function definition | Return type on separate line, indented one tab |
| Braces | Allman style (opening brace on new line) |
| Variable declarations | Top of function, before statements |
| Naming | `lowercase_underscores` for functions, `_T` suffix for types |
| Feature guards | `#ifdef FEAT_*` |
| Translatable strings | `N_("...")` |
| Modeline | Required in every `.c` / `.h` file |
| Prototype generation | `src/proto/*.pro` files |

---

*This document describes MNV 10.0 as of build 287 (2026-04-03).*
