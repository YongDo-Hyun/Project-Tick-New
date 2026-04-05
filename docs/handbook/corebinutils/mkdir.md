# mkdir — Make Directories

## Overview

`mkdir` creates directories with specified permissions. It shares the
`mode_compile()` / `mode_apply()` engine with `chmod` for parsing
symbolic and numeric mode specifications. With `-p`, it creates all
missing intermediate directories.

**Source**: `mkdir/mkdir.c`, `mkdir/mode.c`, `mkdir/mode.h` (shared with chmod)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
mkdir [-pv] [-m mode] directory ...
```

## Options

| Flag | Description |
|------|-------------|
| `-m mode` | Set permissions (numeric or symbolic) |
| `-p` | Create parent directories as needed |
| `-v` | Print each directory as it is created |

## Source Analysis

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options and iterate over arguments |
| `create_single_path()` | Create one directory (no `-p`) |
| `create_parents_path()` | Create directory with parents (`-p`) |
| `create_component()` | Create a single path component |
| `mkdir_with_umask()` | Atomically apply umask during `mkdir(2)` |
| `existing_directory()` | Check if a path already exists as a directory |
| `current_umask()` | Atomically read the current umask |
| `mode_compile()` | Parse mode string to command array (shared) |
| `mode_apply()` | Apply compiled mode to existing permissions |

### Simple Creation

```c
static int
create_single_path(const char *path, mode_t mode)
{
    if (mkdir(path, mode) < 0) {
        error_errno("cannot create directory '%s'", path);
        return 1;
    }

    /* If explicit mode was specified, chmod to override umask */
    if (explicit_mode) {
        if (chmod(path, mode) < 0) {
            error_errno("cannot set permissions on '%s'", path);
            return 1;
        }
    }

    if (verbose)
        printf("mkdir: created directory '%s'\n", path);

    return 0;
}
```

### Parent Directory Creation

```c
static int
create_parents_path(const char *path, mode_t mode,
                    mode_t intermediate_mode)
{
    char *buf = strdup(path);
    char *p = buf;

    /* Skip leading slashes */
    while (*p == '/') p++;

    /* Create each component */
    while (*p) {
        char *slash = strchr(p, '/');
        if (slash) *slash = '\0';

        if (!existing_directory(buf)) {
            if (mkdir_with_umask(buf, intermediate_mode) < 0) {
                if (errno != EEXIST) {
                    error_errno("cannot create '%s'", buf);
                    return 1;
                }
            }
            if (verbose)
                printf("mkdir: created directory '%s'\n", buf);
        }

        if (slash) {
            *slash = '/';
            p = slash + 1;
        } else {
            break;
        }
    }

    /* Apply final mode to the leaf directory */
    if (chmod(buf, mode) < 0) { ... }
    return 0;
}
```

### Atomic Umask Handling

To prevent race conditions when setting permissions:

```c
static mode_t
current_umask(void)
{
    /* Atomically read umask by setting and restoring */
    mode_t mask = umask(0);
    umask(mask);
    return mask;
}

static int
mkdir_with_umask(const char *path, mode_t mode)
{
    /* Use more restrictive intermediate perms:
     * u+wx so the creator can write subdirs */
    mode_t old = umask(0);
    int ret = mkdir(path, mode);
    umask(old);
    return ret;
}
```

Intermediate directories are created with `0300 | mode` to ensure the
creating user always has write and execute access to create children,
even if the specified mode is more restrictive.

### Mode Compilation (Shared with chmod)

```c
/* Numeric modes */
mkdir -m 755 mydir
/* → mode_compile("755") returns compiled bitcmd array */

/* Symbolic modes */
mkdir -m u=rwx,g=rx,o=rx mydir
/* → mode_compile("u=rwx,g=rx,o=rx") */

/* Default mode */
/* 0777 & ~umask (typically 0755) */
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `mkdir(2)` | Create directory |
| `chmod(2)` | Set final permissions |
| `umask(2)` | Read/set file creation mask |
| `stat(2)` | Check if path exists |

## Examples

```sh
# Simple directory
mkdir mydir

# With specific permissions
mkdir -m 700 private_dir

# Create parent directories
mkdir -p /opt/myapp/lib/plugins

# Verbose
mkdir -pv a/b/c
# mkdir: created directory 'a'
# mkdir: created directory 'a/b'
# mkdir: created directory 'a/b/c'

# Symbolic mode
mkdir -m u=rwx,g=rx,o= restricted_dir
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | All directories created successfully |
| 1    | Error creating one or more directories |
