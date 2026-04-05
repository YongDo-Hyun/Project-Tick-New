# cgit — Code Style and Conventions

## Overview

cgit follows C99 conventions with a style influenced by the Linux kernel and
Git project coding standards.  This document describes the patterns, naming
conventions, and idioms used throughout the codebase.

## Language Standard

cgit is written in C99, compiled with:

```makefile
CGIT_CFLAGS += -std=c99
```

No C11 or GNU extensions are required, though some platform-specific features
(like `sendfile()` on Linux) are conditionally compiled.

## Formatting

### Indentation

- Tabs for indentation (1 tab = 8 spaces display width, consistent with
  Linux kernel/Git style)
- No spaces for indentation alignment

### Braces

K&R style (opening brace on same line):

```c
if (condition) {
    /* body */
} else {
    /* body */
}

static void function_name(int arg)
{
    /* function body */
}
```

Functions place the opening brace on its own line.  Control structures
(`if`, `for`, `while`, `switch`) keep it on the same line.

### Line Length

No strict limit, but lines generally stay under 80 characters.  Long function
calls are broken across lines.

## Naming Conventions

### Functions

Public API functions use the `cgit_` prefix:

```c
void cgit_print_commit(const char *rev, const char *prefix);
void cgit_print_diff(const char *new_rev, const char *old_rev, ...);
struct cgit_repo *cgit_add_repo(const char *url);
struct cgit_repo *cgit_get_repoinfo(const char *url);
int cgit_parse_snapshots_mask(const char *str);
```

Static (file-local) functions use descriptive names without prefix:

```c
static void config_cb(const char *name, const char *value);
static void querystring_cb(const char *name, const char *value);
static void process_request(void);
static int open_slot(struct cache_slot *slot);
```

### Types

Struct types use `cgit_` prefix with snake_case:

```c
struct cgit_context;
struct cgit_repo;
struct cgit_config;
struct cgit_query;
struct cgit_page;
struct cgit_environment;
struct cgit_cmd;
struct cgit_filter;
struct cgit_snapshot_format;
```

### Macros and Constants

Uppercase with underscores:

```c
#define ABOUT_FILTER   0
#define COMMIT_FILTER  1
#define SOURCE_FILTER  2
#define EMAIL_FILTER   3
#define AUTH_FILTER    4
#define DIFF_UNIFIED   0
#define DIFF_SSDIFF    1
#define DIFF_STATONLY  2
#define FMT_BUFS       8
#define FMT_SIZE       8192
```

### Variables

Global variables use descriptive names:

```c
struct cgit_context ctx;
struct cgit_repolist cgit_repolist;
const char *cgit_version;
```

## File Organization

### Header Files

Each module has a corresponding header file with include guards:

```c
#ifndef UI_DIFF_H
#define UI_DIFF_H

extern void cgit_print_diff(const char *new_rev, const char *old_rev,
                            const char *prefix, int show_ctrls, int raw);
extern void cgit_print_diffstat(const struct object_id *old,
                                const struct object_id *new,
                                const char *prefix);

#endif /* UI_DIFF_H */
```

### Source Files

Typical source file structure:

1. License header (if present)
2. Include directives
3. Static (file-local) variables
4. Static helper functions
5. Public API functions

### Module Pattern

UI modules follow a consistent pattern with `ui-*.c` / `ui-*.h` pairs:

```c
/* ui-example.c */
#include "cgit.h"
#include "ui-example.h"
#include "html.h"
#include "ui-shared.h"

static void helper_function(void)
{
    /* ... */
}

void cgit_print_example(void)
{
    /* main entry point */
}
```

## Common Patterns

### Global Context

cgit uses a single global `struct cgit_context ctx` variable that holds all
request state.  Functions access it directly rather than passing it as a
parameter:

```c
/* Access global context directly */
if (ctx.repo && ctx.repo->enable_blame)
    cgit_print_blame();

/* Not: cgit_print_blame(&ctx) */
```

### Callback Functions

Configuration and query parsing use callback function pointers:

```c
typedef void (*configfile_value_fn)(const char *name, const char *value);
typedef void (*filepair_fn)(struct diff_filepair *pair);
typedef void (*linediff_fn)(char *line, int len);
typedef void (*cache_fill_fn)(void *cbdata);
```

### String Formatting

The `fmt()` ring buffer is used for temporary string construction:

```c
const char *url = fmt("%s/%s/", ctx.cfg.virtual_root, repo->url);
html_attr(url);
```

Never store `fmt()` results long-term — use `fmtalloc()` or `xstrdup()`.

### NULL Checks

Functions generally check for NULL pointers at the start:

```c
void cgit_print_blob(const char *hex, const char *path,
                     const char *head, int file_only)
{
    if (!hex && !path) {
        cgit_print_error_page(400, "Bad request",
            "Need either hex or path");
        return;
    }
    /* ... */
}
```

### Memory Management

cgit uses Git's `xmalloc` / `xstrdup` / `xrealloc` wrappers that die on
allocation failure:

```c
char *name = xstrdup(value);
repo = xrealloc(repo, new_size);
```

No explicit `free()` calls in most paths — the CGI process exits after each
request, and the OS reclaims all memory.

### Boolean as Int

Boolean values are represented as `int` (0 or 1), consistent with C99
convention before `_Bool`:

```c
int enable_blame;
int enable_commit_graph;
int binary;
int match;
```

### Typedef Avoidance

Structs are generally not typedef'd — they use the `struct` keyword
explicitly:

```c
struct cgit_repo *repo;
struct cache_slot slot;
```

Exception: function pointer typedefs are used for callbacks:

```c
typedef void (*configfile_value_fn)(const char *name, const char *value);
```

## Error Handling

### `die()` for Fatal Errors

Unrecoverable errors use Git's `die()`:

```c
if (!ctx.repo)
    die("no repository");
```

### Error Pages for User Errors

User-facing errors use the error page function:

```c
cgit_print_error_page(404, "Not Found",
    "No repository found for '%s'",
    ctx.qry.repo);
```

### Return Codes

Functions that can fail return int (0 = success, non-zero = error):

```c
static int open_slot(struct cache_slot *slot)
{
    slot->cache_fd = open(slot->cache_name, O_RDONLY);
    if (slot->cache_fd == -1)
        return errno;
    return 0;
}
```

## Preprocessor Usage

Conditional compilation for platform features:

```c
#ifdef HAVE_LINUX_SENDFILE
    sendfile(STDOUT_FILENO, slot->cache_fd, &off, size);
#else
    /* read/write fallback */
#endif

#ifdef HAVE_LUA
    /* Lua filter support */
#endif
```

## Git Library Integration

cgit includes Git as a library.  It uses Git's internal APIs directly:

```c
#include "git/cache.h"
#include "git/object.h"
#include "git/commit.h"
#include "git/diff.h"
#include "git/revision.h"
#include "git/archive.h"
```

Functions from Git's library are called without wrapper layers:

```c
struct commit *commit = lookup_commit_reference(&oid);
struct tree *tree = parse_tree_indirect(&oid);
init_revisions(&rev, NULL);
```

## Documentation

- Code comments are used sparingly, mainly for non-obvious logic
- No Doxygen or similar documentation generators are used
- Function documentation is in the header files as prototypes with
  descriptive parameter names
- The `cgitrc.5.txt` file provides user-facing documentation in
  man page format

## Commit Messages

Commit messages follow the standard Git format:

```
subject: brief description (50 chars or less)

Extended description wrapping at 72 characters.  Explain what and why,
not how.
```
