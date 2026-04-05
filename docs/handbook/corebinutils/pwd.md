# pwd — Print Working Directory

## Overview

`pwd` prints the absolute pathname of the current working directory.
It supports logical mode (using `$PWD`) and physical mode (resolving
symlinks). Logical mode is the default, with fallback to physical
if validation fails.

**Source**: `pwd/pwd.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
pwd [-L | -P]
```

## Options

| Flag | Description |
|------|-------------|
| `-L` | Logical: use `$PWD` (default) |
| `-P` | Physical: resolve all symlinks |

When both are specified, the last one wins.

## Source Analysis

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options and dispatch |
| `getcwd_logical()` | Validate and use `$PWD` |
| `getcwd_physical()` | Resolve via `getcwd(3)` |
| `usage()` | Print usage message |

### Logical Mode (Default)

```c
static char *
getcwd_logical(void)
{
    const char *pwd = getenv("PWD");

    /* Must be set and absolute */
    if (!pwd || pwd[0] != '/')
        return NULL;

    /* Must not contain "." or ".." components */
    if (contains_dot_components(pwd))
        return NULL;

    /* Must refer to the same directory as "." */
    struct stat pwd_sb, dot_sb;
    if (stat(pwd, &pwd_sb) < 0 || stat(".", &dot_sb) < 0)
        return NULL;
    if (pwd_sb.st_dev != dot_sb.st_dev ||
        pwd_sb.st_ino != dot_sb.st_ino)
        return NULL;

    return strdup(pwd);
}
```

The `$PWD` validation ensures:
1. The value is an absolute path
2. It contains no `.` or `..` components
3. The path resolves to the same inode as `.`

### Physical Mode

```c
static char *
getcwd_physical(void)
{
    /* POSIX: getcwd(NULL, 0) dynamically allocates */
    return getcwd(NULL, 0);
}
```

Uses the POSIX extension `getcwd(NULL, 0)` which allocates the
returned buffer dynamically, avoiding fixed-size buffer limitations.

### Main Logic

```c
int main(int argc, char *argv[])
{
    int mode = MODE_LOGICAL;  /* Default: logical */

    while ((ch = getopt(argc, argv, "LP")) != -1) {
        switch (ch) {
        case 'L': mode = MODE_LOGICAL; break;
        case 'P': mode = MODE_PHYSICAL; break;
        default:  usage();
        }
    }

    char *cwd;
    if (mode == MODE_LOGICAL) {
        cwd = getcwd_logical();
        if (!cwd)
            cwd = getcwd_physical();  /* Fallback */
    } else {
        cwd = getcwd_physical();
    }

    if (!cwd)
        err(1, "getcwd");

    puts(cwd);
    free(cwd);
    return 0;
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `getcwd(3)` | Physical working directory |
| `stat(2)` | Validate `$PWD` against `.` |
| `getenv(3)` | Read `$PWD` environment variable |

## Examples

```sh
# Default (logical)
pwd
# /home/user/projects/mylink  (preserves symlink name)

# Physical
pwd -P
# /home/user/actual/path  (resolved symlinks)

# Demonstrate difference
cd /tmp
ln -s /usr/local/share mylink
cd mylink
pwd -L   # → /tmp/mylink
pwd -P   # → /usr/local/share
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Error (cannot determine directory) |
