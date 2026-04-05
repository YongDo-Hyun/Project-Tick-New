# ls — List Directory Contents

## Overview

`ls` lists files and directory contents with extensive formatting,
sorting, filtering, and colorization options. This implementation uses
the Linux `statx(2)` syscall for file metadata (including birth time),
and provides column, long, stream, and single-column layout modes.

**Source**: `ls/ls.c`, `ls/ls.h`, `ls/print.c`, `ls/cmp.c`, `ls/util.c`,
`ls/extern.h`
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
ls [-ABCFGHILPRSTUWabcdfghiklmnopqrstuvwxy1,] [--color[=when]]
   [--group-directories-first] [file ...]
```

## Source Architecture

### File Responsibilities

| File | Purpose | Key Functions |
|------|---------|---------------|
| `ls.c` | Main, option parsing, directory traversal | `main()`, `parse_options()`, `collect_directory_entries()` |
| `ls.h` | Type definitions, enums, structs | `layout_mode`, `sort_mode`, `time_field` |
| `print.c` | Output formatting for all layout modes | `printlong()`, `printcol()`, `printstream()` |
| `cmp.c` | Sorting comparators | `namecmp()`, `mtimecmp()`, `sizecmp()` |
| `util.c` | Helper functions | `emalloc()`, `printescaped()` |
| `extern.h` | Function prototypes across files | All cross-file declarations |

### Enums (ls.h)

```c
enum layout_mode {
    SINGLE,    /* One file per line (-1) */
    COLUMNS,   /* Multi-column (-C) */
    LONG,      /* Long listing (-l) */
    STREAM,    /* Comma-separated (-m) */
};

enum sort_mode {
    BY_NAME,       /* Default alphabetical */
    BY_TIME,       /* -t: modification/access/birth/change time */
    BY_SIZE,       /* -S: file size */
    BY_VERSION,    /* --sort=version: version number sort */
    UNSORTED,      /* -f: no sorting */
};

enum time_field {
    MTIME,    /* Modification time (default) */
    ATIME,    /* Access time (-u) */
    BTIME,    /* Birth/creation time (-U) */
    CTIME,    /* Inode change time (-c) */
};

enum follow_mode {
    FOLLOW_NEVER,    /* -P: never follow symlinks */
    FOLLOW_ALWAYS,   /* -L: always follow */
    FOLLOW_CMDLINE,  /* -H: follow on command line only */
};

enum color_mode {
    COLOR_NEVER,
    COLOR_ALWAYS,
    COLOR_AUTO,      /* Only when stdout is a TTY */
};
```

### File Time Struct

```c
struct file_time {
    struct timespec ts;
    bool available;     /* False if filesystem doesn't support it */
};
```

### statx(2) Integration

Since musl libc may not provide `statx` wrappers, `ls` defines the
syscall interface inline:

```c
static int
linux_statx(int dirfd, const char *path, int flags,
            unsigned int mask, struct statx *stx)
{
    return syscall(__NR_statx, dirfd, path, flags, mask, stx);
}
```

This enables birth time (`btime`) on filesystems that support it
(ext4, btrfs, XFS) where traditional `stat(2)` does not expose it.

### Option Parsing

```c
static const char *optstring =
    "ABCFGHILPRSTUWabcdfghiklmnopqrstuvwxy1,";

static void
parse_options(int argc, char *argv[])
{
    /* Short options via getopt(3) */
    while ((ch = getopt_long(argc, argv, optstring,
                             long_options, NULL)) != -1) {
        switch (ch) {
        case 'l': layout = LONG; break;
        case 'C': layout = COLUMNS; break;
        case '1': layout = SINGLE; break;
        case 'm': layout = STREAM; break;
        case 't': sort = BY_TIME; break;
        case 'S': sort = BY_SIZE; break;
        case 'r': reverse = true; break;
        case 'a': show_hidden = ALL; break;
        case 'A': show_hidden = ALMOST_ALL; break;
        case 'R': recurse = true; break;
        /* ... more options ... */
        }
    }
}
```

### Long Options

| Long Option | Description |
|-------------|-------------|
| `--color[=when]` | Colorize output (always/auto/never) |
| `--group-directories-first` | Sort directories before files |
| `--sort=version` | Version-number sort |

### Directory Traversal

```c
static void
collect_directory_entries(const char *dir, struct entry_list *list)
{
    DIR *dp = opendir(dir);
    struct dirent *ent;

    while ((ent = readdir(dp)) != NULL) {
        /* Skip . and .. (unless -a) */
        if (!show_hidden && ent->d_name[0] == '.')
            continue;

        struct entry *e = alloc_entry(ent->d_name);
        stat_with_policy(dir, e);
        list_append(list, e);
    }
    closedir(dp);
}
```

### Recursive Listing

```c
static void
list_directory(const char *path, int depth)
{
    collect_directory_entries(path, &entries);
    sort_entries(&entries);
    display_entries(&entries);

    if (recurse) {
        for (each entry that is a directory) {
            if (should_recurse(entry)) {
                /* Cycle detection: check device/inode */
                if (visit_stack_contains(entry->ino, entry->dev))
                    warnx("cycle detected: %s", path);
                else
                    list_directory(full_path, depth + 1);
            }
        }
    }
}
```

### Birth Time via statx

```c
static void
fill_birthtime(struct entry *e, const struct statx *stx)
{
    if (stx->stx_mask & STATX_BTIME) {
        e->btime.ts.tv_sec = stx->stx_btime.tv_sec;
        e->btime.ts.tv_nsec = stx->stx_btime.tv_nsec;
        e->btime.available = true;
    } else {
        e->btime.available = false;
    }
}
```

### Sorting (cmp.c)

Comparators are selected based on the sort mode and direction:

```c
int namecmp(const struct entry *a, const struct entry *b);
int mtimecmp(const struct entry *a, const struct entry *b);
int atimecmp(const struct entry *a, const struct entry *b);
int btimecmp(const struct entry *a, const struct entry *b);
int ctimecmp(const struct entry *a, const struct entry *b);
int sizecmp(const struct entry *a, const struct entry *b);
```

All comparators fall back to `namecmp()` for stable ordering when
primary keys are equal.

### Output Formatting (print.c)

| Function | Layout Mode |
|----------|-------------|
| `printlong()` | `-l` long listing with permissions, owner, size, date |
| `printcol()` | `-C` multi-column (default for TTY) |
| `printstream()` | `-m` comma-separated stream |
| `printsingle()` | `-1` one per line (default for pipe) |

Human-readable sizes (`-h`) format with K, M, G, T suffixes.

## Full Options Reference

| Flag | Description |
|------|-------------|
| `-a` | Show all entries (including `.` and `..`) |
| `-A` | Show almost all (exclude `.` and `..`) |
| `-b` | Print C-style escapes for non-printable chars |
| `-C` | Multi-column output (default if TTY) |
| `-c` | Use ctime (inode change time) for sorting/display |
| `-d` | List directories themselves, not contents |
| `-F` | Append type indicator (`/`, `*`, `@`, `=`, `%`, `\|`) |
| `-f` | Unsorted, show all |
| `-G` | Colorize output (same as `--color=auto`) |
| `-g` | Long format without owner |
| `-H` | Follow symlinks on command line |
| `-h` | Human-readable sizes |
| `-I` | Suppress auto-column mode |
| `-i` | Print inode number |
| `-k` | Use 1024-byte blocks |
| `-L` | Follow all symlinks |
| `-l` | Long listing format |
| `-m` | Stream (comma-separated) output |
| `-n` | Numeric UID/GID |
| `-o` | Long format without group |
| `-P` | Never follow symlinks |
| `-p` | Append `/` to directories |
| `-q` | Replace non-printable with `?` |
| `-R` | Recursive listing |
| `-r` | Reverse sort order |
| `-S` | Sort by size (largest first) |
| `-s` | Print block count |
| `-T` | Show complete time information |
| `-t` | Sort by time |
| `-U` | Use birth time |
| `-u` | Use access time |
| `-v` | Sort version numbers naturally |
| `-w` | Force raw (non-printable) output |
| `-x` | Multi-column sorted across |
| `-y` | Sort by extension |
| `-1` | One entry per line |
| `,` | Thousands separator in sizes |

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `statx(2)` | File metadata including birth time |
| `stat(2)` / `lstat(2)` | Fallback file metadata |
| `opendir(3)` / `readdir(3)` | Directory enumeration |
| `readlink(2)` | Resolve symlink targets |
| `ioctl(TIOCGWINSZ)` | Terminal width detection |
| `isatty(3)` | Detect if stdout is a terminal |
| `getpwuid(3)` / `getgrgid(3)` | User/group name lookup |

## Linux-Specific Notes

- Uses `statx(2)` directly via `syscall()` for birth time support
- Defines `struct statx` inline for musl compatibility
- No BSD file flags (`-o`, `-W` not supported)
- No MAC label support (`-Z` not supported)

## Examples

```sh
# Long listing
ls -la

# Human-readable, sorted by size
ls -lhS

# Recursive with color
ls -R --color=auto

# Sort by modification time
ls -lt

# Show birth time (on supporting filesystems)
ls -lU

# Directories first
ls --group-directories-first
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Minor problem (cannot access one file) |
| 2    | Serious trouble (cannot access command line argument) |
