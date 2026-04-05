# chmod — Change File Permissions

## Overview

`chmod` changes the file mode (permission) bits of specified files. It supports
both symbolic and numeric (octal) mode specifications, recursive directory
traversal, symlink handling policies, ACL awareness, and verbose operation.

**Source**: `chmod/chmod.c`, `chmod/mode.c`, `chmod/mode.h`
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
chmod [-fhvR [-H | -L | -P]] mode file ...
```

## Options

| Flag | Description |
|------|-------------|
| `-R` | Recursive: change files and directories recursively |
| `-H` | Follow symlinks on the command line (with `-R` only) |
| `-L` | Follow all symbolic links (with `-R` only) |
| `-P` | Do not follow symbolic links (default with `-R`) |
| `-f` | Force: suppress most error messages |
| `-h` | Affect symlinks themselves, not their targets |
| `-v` | Verbose: print changed files |
| `-vv` | Very verbose: print all files, whether changed or not |

## Source Analysis

### chmod.c — Main Implementation

#### Key Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options via `getopt(3)`, compile mode, dispatch traversal |
| `walk_path()` | Stat a path and decide how to process it |
| `walk_dir()` | Enumerate directory contents for recursive processing |
| `apply_mode()` | Compile mode, apply via `fchmodat(2)`, report changes |
| `stat_path()` | Wrapper choosing between `stat(2)` and `lstat(2)` |
| `should_skip_acl_check()` | Cache per-filesystem ACL support detection |
| `visited_push()` / `visited_check()` | Cycle detection via device/inode tracking |
| `siginfo_handler()` | Handle SIGINFO/SIGUSR1 for progress reporting |
| `join_path()` | Safe path concatenation with separator handling |

#### Option Processing

```c
while ((ch = getopt(argc, argv, "HLPRfhv")) != -1)
    switch (ch) {
    case 'H': Hflag = 1; Lflag = 0; break;
    case 'L': Lflag = 1; Hflag = 0; break;
    case 'P': Hflag = Lflag = 0; break;
    case 'R': Rflag = 1; break;
    case 'f': fflag = 1; break;
    case 'h': hflag = 1; break;
    case 'v': vflag++; break;  /* -v increments, -vv = 2 */
    default:  usage();
    }
```

#### Recursive Traversal

The `-R` flag triggers recursive directory traversal. `chmod` implements its
own traversal with cycle detection rather than using `fts(3)`:

```c
static int
walk_dir(const char *dir_path, const struct chmod_options *opts)
{
    DIR *dp;
    struct dirent *de;
    char *child_path;
    int ret = 0;

    dp = opendir(dir_path);
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        child_path = join_path(dir_path, de->d_name);
        ret |= walk_path(child_path, opts, false);
        free(child_path);
    }
    closedir(dp);
    return ret;
}
```

#### Cycle Detection

To prevent infinite traversal through symlink loops or bind mounts, `chmod`
maintains a visited-path stack keyed on `(dev, ino)` pairs:

```c
static int visited_check(dev_t dev, ino_t ino);   /* returns 1 if seen */
static void visited_push(dev_t dev, ino_t ino);   /* record as visited */
```

### mode.c — Mode Parsing Library

#### Data Types

```c
typedef struct {
    int cmd;     /* '+', '-', '=', 'X', 'u', 'g', 'o' */
    mode_t bits; /* Permission bits to modify */
    mode_t who;  /* Scope mask (user/group/other/all) */
} bitcmd_t;
```

#### Key Functions

| Function | Purpose |
|----------|---------|
| `mode_compile()` | Parse mode string into array of `bitcmd_t` operations |
| `mode_apply()` | Apply compiled mode to an existing `mode_t` value |
| `mode_free()` | Free compiled mode array |
| `strmode()` | Convert `mode_t` to display string like `"drwxr-xr-x "` |
| `get_current_umask()` | Atomically read process umask |

#### Numeric Mode Parsing

Numeric modes are parsed as octal:

```c
if (isdigit(*mode_string)) {
    /* Parse octal: 755 → rwxr-xr-x, 0644 → rw-r--r-- */
    val = strtol(mode_string, &ep, 8);
    /* Set bits directly, clearing old permission bits */
}
```

#### Symbolic Mode Parsing

Symbolic modes follow the grammar:

```
mode    ::= clause [, clause ...]
clause  ::= [who ...] [action ...] action
who     ::= 'u' | 'g' | 'o' | 'a'
action  ::= op [perm ...]
op      ::= '+' | '-' | '='
perm    ::= 'r' | 'w' | 'x' | 'X' | 's' | 't' | 'u' | 'g' | 'o'
```

Examples:
- `u+rwx` — Add read/write/execute for user
- `go-w` — Remove write for group and other
- `a=rx` — Set all to read+execute only
- `u=g` — Copy group permissions to user
- `+X` — Add execute only if already executable or is a directory
- `u+s` — Set SUID bit
- `g+s` — Set SGID bit
- `+t` — Set sticky bit

#### The 'X' Permission

The conditional execute permission `X` is a special case:

```c
/* 'X' only adds execute if:
 *   - The file is a directory, OR
 *   - Any execute bit is already set */
if (cmd == 'X') {
    if (S_ISDIR(old_mode) || (old_mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
        /* apply execute bits */
}
```

This is commonly used with `-R` to make directories traversable without
making regular files executable: `chmod -R u+rwX,go+rX dir/`

#### Mode Compilation

The `mode_compile()` function translates a mode string into an array of
`bitcmd_t` instructions that can be applied to any `mode_t`:

```c
bitcmd_t *mode_compile(const char *mode_string);

/* Usage: */
bitcmd_t *set = mode_compile("u+rw,go+r");
mode_t new_mode = mode_apply(set, old_mode);
mode_free(set);
```

This two-phase approach lets the mode be parsed once and applied to many
files during recursive traversal.

#### strmode() Function

Converts a numeric `mode_t` into a human-readable string:

```c
char buf[12];
strmode(0100755, buf);  /* "drwxr-xr-x " → for directories */
strmode(0100644, buf);  /* "-rw-r--r-- " → for regular files */
```

The output is always 11 characters: type + 9 permission chars + space.

### Umask Interaction

When no scope (`u`, `g`, `o`, `a`) is specified in a symbolic mode, the
umask determines which bits are affected. The umask is read atomically:

```c
static mode_t
get_current_umask(void)
{
    mode_t mask;
    sigset_t set, oset;

    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, &oset);
    mask = umask(0);
    umask(mask);
    sigprocmask(SIG_SETMASK, &oset, NULL);
    return mask;
}
```

Signals are blocked during the read-restore cycle to prevent another
thread or signal handler from seeing a zero umask.

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `fchmodat(2)` | Apply permission changes |
| `fstatat(2)` | Get current file mode |
| `lstat(2)` | Stat without following symlinks |
| `opendir(3)` / `readdir(3)` | Directory traversal |
| `sigaction(2)` | Install SIGINFO handler |
| `umask(2)` | Read current umask |

## ACL Integration

`chmod` is aware of POSIX ACLs. When changing permissions on a file with
ACLs, the ACL mask entry may need updating. The `should_skip_acl_check()`
function caches whether a filesystem supports ACLs to avoid repeated
`pathconf()` calls:

```c
static bool
should_skip_acl_check(const char *path)
{
    /* Cache per-device ACL support to avoid pathconf() on every file */
}
```

## Examples

```sh
# Set exact permissions
chmod 755 script.sh
chmod 0644 config.txt

# Add execute for user
chmod u+x program

# Recursive: directories traversable, files not executable
chmod -R u+rwX,go+rX project/

# Remove write for everyone except owner
chmod go-w important.txt

# Copy group permissions to other
chmod o=g shared_file

# Set SUID
chmod u+s /usr/local/bin/helper

# Verbose mode
chmod -Rv 755 bin/
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | All files changed successfully |
| 1    | Error changing one or more files (partial failure) |

## Differences from GNU chmod

- No `--reference=FILE` option
- No `--changes` (use `-v`)
- `-h` flag affects symlinks (GNU uses `--no-dereference`)
- `-vv` for very verbose (GNU only has one `-v` level)
- ACL awareness is filesystem-dependent
- Mode compiler supports `u=g` (copy from group to user)
