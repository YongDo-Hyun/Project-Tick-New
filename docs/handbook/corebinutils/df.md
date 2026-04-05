# df — Display Filesystem Space Usage

## Overview

`df` reports total, used, and available disk space for mounted filesystems.
It supports multiple output formats (POSIX, human-readable, SI), filesystem
type filtering, inode display, and Linux-native mount information parsing.

**Source**: `df/df.c` (single file, 100+ functions)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
df [-abcgHhiklmPTtx] [-t type] [file ...]
```

## Options

| Flag | Description |
|------|-------------|
| `-a` | Show all filesystems including zero-size ones |
| `-b` | Display sizes in 512-byte blocks |
| `-c` | Print total line at end |
| `-g` | Display sizes in gigabytes |
| `-H` | Human-readable with SI units (1000-based) |
| `-h` | Human-readable with binary units (1024-based) |
| `-i` | Show inode information instead of block usage |
| `-k` | Display sizes in kilobytes |
| `-l` | Show only local (non-remote) filesystems |
| `-m` | Display sizes in megabytes |
| `-P` | POSIX output format (one line per filesystem) |
| `-T` | Show filesystem type column |
| `-t type` | Filter to specified filesystem type |
| `-x` | Exclude specified filesystem type |
| `,` | Use thousands separator in output |

## Source Analysis

### Key Data Structures

```c
struct options {
    bool show_all;
    bool show_inodes;
    bool show_type;
    bool posix_format;
    bool human_readable;
    bool si_units;
    bool local_only;
    bool show_total;
    bool thousands_separator;
    /* block size settings from -b/-k/-m/-g or BLOCKSIZE env */
};

struct mount_entry {
    char *source;         /* Device path (/dev/sda1) */
    char *target;         /* Mount point (/home) */
    char *fstype;         /* Filesystem type (ext4, tmpfs) */
    char *options;        /* Mount options (rw,noatime) */
    dev_t device;         /* Device number */
};

struct mount_table {
    struct mount_entry *entries;
    size_t count;
    size_t capacity;
};

struct row {
    char *filesystem;     /* Formatted filesystem column */
    char *type;           /* Filesystem type */
    char *size;           /* Total size */
    char *used;           /* Used space */
    char *avail;          /* Available space */
    char *capacity;       /* Percentage used */
    char *mount_point;    /* Mount point path */
    char *iused;          /* Inodes used */
    char *ifree;          /* Inodes free */
};

struct column_widths {
    int filesystem;
    int type;
    int size;
    int used;
    int avail;
    int capacity;
    int mount_point;
};
```

### Linux-Native Mount Parsing

Unlike BSD which uses `getmntinfo(3)` / `statfs(2)`, this port reads
`/proc/self/mountinfo` directly:

```c
static int
parse_mountinfo(struct mount_table *table)
{
    FILE *fp = fopen("/proc/self/mountinfo", "r");
    /* Parse each line:
     * ID PARENT_ID MAJOR:MINOR ROOT MOUNT_POINT OPTIONS ... - FSTYPE SOURCE SUPER_OPTIONS
     */
    while (getline(&line, &linesz, fp) != -1) {
        /* Extract fields, unescape special characters */
        entry.source = unescape_mountinfo(source_str);
        entry.target = unescape_mountinfo(target_str);
        entry.fstype = strdup(fstype_str);
    }
}
```

#### Escape Handling

Mount paths in `/proc/self/mountinfo` use octal escapes for special
characters (spaces, newlines, backslashes):

```c
static char *
unescape_mountinfo(const char *text)
{
    /* Convert \040 → space, \011 → tab, \012 → newline, \134 → backslash */
}
```

### Filesystem Stats

`df` uses `statvfs(2)` instead of BSD's `statfs(2)`:

```c
struct statvfs sv;
if (statvfs(mount_point, &sv) != 0)
    return -1;

total_blocks = sv.f_blocks;
free_blocks  = sv.f_bfree;
avail_blocks = sv.f_bavail;  /* Available to unprivileged users */
block_size   = sv.f_frsize;

total_inodes = sv.f_files;
free_inodes  = sv.f_ffree;
```

### Remote Filesystem Detection

The `-l` (local only) flag requires distinguishing local from remote
filesystems:

```c
static bool
is_remote_filesystem(const struct mount_entry *entry)
{
    /* Check filesystem type */
    if (strcmp(entry->fstype, "nfs") == 0 ||
        strcmp(entry->fstype, "nfs4") == 0 ||
        strcmp(entry->fstype, "cifs") == 0 ||
        strcmp(entry->fstype, "smbfs") == 0 ||
        strcmp(entry->fstype, "fuse.sshfs") == 0)
        return true;

    /* Check source for remote indicators (host:path or //host/share) */
    if (strchr(entry->source, ':') != NULL)
        return true;
    if (entry->source[0] == '/' && entry->source[1] == '/')
        return true;

    return false;
}
```

### Human-Readable Formatting

```c
static char *
format_human_readable(uint64_t bytes, bool si)
{
    unsigned int base = si ? 1000 : 1024;
    const char *const *units = si ? si_units : binary_units;
    /* Scale and format: "1.5G", "234M", "45K" */
}
```

### BLOCKSIZE Environment

The `BLOCKSIZE` environment variable can override the default block size:

```c
char *bs = getenv("BLOCKSIZE");
if (bs != NULL) {
    /* Parse: "512", "K", "M", "G", or "1k", "4k", etc. */
}
```

### Safe Integer Arithmetic

`df` performs arithmetic with overflow protection:

```c
/* Safe multiplication with clamping */
static uint64_t
safe_mul(uint64_t a, uint64_t b)
{
    if (a != 0 && b > UINT64_MAX / a)
        return UINT64_MAX;  /* Clamp instead of overflow */
    return a * b;
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `statvfs(2)` | Query filesystem statistics |
| `stat(2)` | Identify filesystem for file arguments |
| `open(2)` / `read(2)` | Parse `/proc/self/mountinfo` |

## Examples

```sh
# Default output
df

# Human-readable sizes
df -h

# Show filesystem type
df -hT

# Only local filesystems
df -hl

# POSIX format
df -P

# Inode usage
df -i

# Specific filesystem
df /home

# Total line
df -hc

# Specific filesystem type
df -t ext4
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Error accessing filesystem |

## Differences from GNU df

- Uses `/proc/self/mountinfo` directly (no libmount)
- No `--output` for custom column selection
- `-c` for total line (GNU uses `--total`)
- BLOCKSIZE env var compatibility (BSD convention)
- No `--sync` / `--no-sync` options
