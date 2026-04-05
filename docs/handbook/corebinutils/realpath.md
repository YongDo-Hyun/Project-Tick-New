# realpath — Resolve to Canonical Path

## Overview

`realpath` resolves each given pathname to its canonical absolute form
by expanding all symbolic links, resolving `.` and `..` references,
and removing extra `/` characters.

**Source**: `realpath/realpath.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
realpath [-q] [path ...]
```

## Options

| Flag | Description |
|------|-------------|
| `-q` | Quiet: suppress error messages for non-existent paths |

## Source Analysis

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options and resolve loop |
| `resolve_path()` | Wrapper around `realpath(3)` |
| `set_progname()` | Extract program name from `argv[0]` |
| `print_line()` | Safe stdout writing |
| `usage()` | Print usage message |
| `warnx_msg()` | Warning without errno |
| `warn_path_errno()` | Warning with errno for path |

### Core Logic

```c
static int
resolve_path(const char *path, bool quiet)
{
    char *resolved = realpath(path, NULL);
    if (!resolved) {
        if (!quiet)
            warn("%s", path);
        return 1;
    }

    puts(resolved);
    free(resolved);
    return 0;
}
```

### Main Loop

```c
int main(int argc, char *argv[])
{
    bool quiet = false;
    int ch, errors = 0;

    while ((ch = getopt(argc, argv, "q")) != -1) {
        switch (ch) {
        case 'q': quiet = true; break;
        default:  usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 0)
        usage();

    for (int i = 0; i < argc; i++)
        errors |= resolve_path(argv[i], quiet);

    return errors ? 1 : 0;
}
```

Uses `realpath(path, NULL)` (POSIX.1-2008) for dynamic buffer
allocation, avoiding `PATH_MAX` limitations.

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `realpath(3)` | Canonicalize pathname |

## Examples

```sh
# Simple resolution
realpath ../foo/bar
# → /home/user/foo/bar

# Resolve symlink
ln -s /usr/local/bin target
realpath target
# → /usr/local/bin

# Quiet mode (no error for missing)
realpath -q /nonexistent/path
# (no output, exit 1)

# Multiple paths
realpath /tmp/../etc ./relative/path
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | All paths resolved successfully |
| 1    | One or more paths could not be resolved |
