# Code Style — Corebinutils Conventions

## Overview

The corebinutils codebase follows FreeBSD kernel style (KNF) with
Linux-specific adaptations. This document catalogs the coding
conventions observed across all utilities.

## File Organization

### Standard File Layout

```c
/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) YYYY Project Tick
 * Copyright (c) YYYY The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution notice ...
 */

/* System headers (alphabetical) */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Local headers */
#include "utility.h"

/* Macros */
#define BUFSIZE  (128 * 1024)

/* Types and structs */
struct options { ... };

/* Static function prototypes */
static void usage(void);

/* Static globals */
static const char *progname;

/* Functions (main last or first, utility-dependent) */
```

### Header Guard Style

```c
/* No #pragma once — uses traditional guards */
#ifndef _DD_H_
#define _DD_H_
/* ... */
#endif /* !_DD_H_ */
```

## Naming Conventions

### Functions

- **Lowercase with underscores**: `parse_args()`, `copy_file_data()`
- **Static for file-scope**: All non-`main` functions are `static` unless
  needed by other translation units
- **Verb-first**: `read_file()`, `write_output()`, `parse_duration()`
- **Predicate prefix**: `is_directory()`, `should_recurse()`, `has_flag()`

### Variables

- **Short names in small scopes**: `n`, `p`, `ch`, `sb`, `dp`
- **Descriptive names in structs**: `suppress_newline`, `follow_mode`
- **Constants as macros**: `BUFSIZE`, `EXIT_TIMEOUT`, `COMMLEN`
- **Global flags**: Single-word or abbreviated: `verbose`, `force`, `rflag`

### Struct Naming

```c
/* Tagged structs (no typedef for most) */
struct options { ... };
struct mount_entry { ... };

/* Typedefs only for opaque or complex types */
typedef struct line line_t;
typedef struct { ... } bitcmd_t;
typedef regex_t pattern_t;
```

## Option Processing

### getopt(3) Pattern

```c
while ((ch = getopt(argc, argv, "fhilnRsvwx")) != -1) {
    switch (ch) {
    case 'f':
        opts.force = true;
        break;
    case 'v':
        opts.verbose = true;
        break;
    /* ... */
    default:
        usage();
    }
}
argc -= optind;
argv += optind;
```

### getopt_long(3) Pattern

```c
static const struct option long_options[] = {
    {"color",                optional_argument, NULL, 'G'},
    {"group-directories-first", no_argument,    NULL, OPT_GROUPDIRS},
    {NULL, 0, NULL, 0},
};

while ((ch = getopt_long(argc, argv, optstring,
                         long_options, NULL)) != -1) { ... }
```

### Manual Parsing (echo)

```c
/* When getopt is too heavy */
while (*argv && strcmp(*argv, "-n") == 0) {
    suppress_newline = true;
    argv++;
}
```

## Error Handling

### BSD err(3) Family

```c
#include <err.h>

err(1, "open '%s'", path);      /* perror-style with exit */
errx(2, "invalid mode: %s", s); /* No errno, with exit */
warn("stat '%s'", path);        /* perror-style, no exit */
warnx("skipping '%s'", path);   /* No errno, no exit */
```

### Custom Error Functions

Many utilities define their own for consistency:

```c
static void
error_errno(const char *fmt, ...)
{
    int saved = errno;
    va_list ap;
    fprintf(stderr, "%s: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", strerror(saved));
}

static void
error_msg(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
```

### Usage Functions

```c
static void __dead2   /* noreturn attribute */
usage(void)
{
    fprintf(stderr, "usage: %s [-fiv] source target\n", progname);
    exit(2);
}
```

## Memory Management

### Dynamic Allocation Patterns

```c
/* Always check allocation */
char *buf = malloc(size);
if (buf == NULL)
    err(1, "malloc");

/* strdup with check */
char *copy = strdup(str);
if (copy == NULL)
    err(1, "strdup");

/* Adaptive buffer sizing */
size_t bufsize = BUFSIZE_MAX;
while (bufsize >= BUFSIZE_MIN) {
    buf = malloc(bufsize);
    if (buf) break;
    bufsize /= 2;
}
```

### No Global malloc/free Tracking

Utilities that process-exit after completion do not free final
allocations — the OS reclaims all memory. Early-exit utilities
(cat, echo, pwd) rely on this.

## Portability Patterns

### Conditional Compilation

```c
/* Feature detection from configure */
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

/* BSD vs Linux */
#ifdef __linux__
    /* Linux-specific path */
#else
    /* BSD fallback (rarely used) */
#endif

/* musl compatibility */
#ifndef STAILQ_HEAD
#define STAILQ_HEAD(name, type) ...
#endif
```

### Inline Syscall Wrappers

```c
/* For syscalls not in musl headers */
static int
linux_statx(int dirfd, const char *path, int flags,
            unsigned int mask, struct statx *stx)
{
    return syscall(__NR_statx, dirfd, path, flags, mask, stx);
}
```

## Formatting

### Indentation

- **Tabs** for indentation (KNF style)
- **8-space tab stops** (standard)
- Continuation lines indented 4 spaces from operator

### Braces

```c
/* K&R for functions */
static void
function_name(int arg)
{
    /* body */
}

/* Same-line for control flow */
if (condition) {
    /* body */
} else {
    /* body */
}

/* No braces for single statements */
if (error)
    return -1;
```

### Line Length

- Target 80 columns
- Long function signatures wrap at parameter boundaries
- Long strings use concatenation

### Switch Statements

```c
switch (ch) {
case 'f':
    force = true;
    break;
case 'v':
    verbose++;
    break;
default:
    usage();
    /* NOTREACHED */
}
```

## Common Macros

```c
/* Array size */
#define nitems(x) (sizeof(x) / sizeof((x)[0]))

/* Min/Max */
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Noreturn */
#define __dead2 __attribute__((__noreturn__))

/* Unused parameter */
#define __unused __attribute__((__unused__))
```

## Signal Handling Conventions

```c
/* Volatile sig_atomic_t for signal flags */
static volatile sig_atomic_t info_requested;

/* Minimal signal handlers (set flag only) */
static void
handler(int sig)
{
    (void)sig;
    info_requested = 1;
}

/* Check flag in main loop */
if (info_requested) {
    report_progress();
    info_requested = 0;
}
```

## Build System Conventions

- Per-utility `GNUmakefile` is the source of truth
- Top-level `GNUmakefile` generated by `configure`
- All object files go to `build/<utility>/`
- Final binaries go to `out/bin/`
- `CFLAGS` include `-Wall -Wextra -Werror` by default
