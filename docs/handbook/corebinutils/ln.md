# ln — Make Links

## Overview

`ln` creates hard links or symbolic links between files. It supports
interactive prompting, forced overwriting, verbose output, and optional
warnings for missing symbolic link targets.

**Source**: `ln/ln.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
ln [-sfhivFLPnw] source_file [target_file]
ln [-sfhivFLPnw] source_file ... target_dir
```

## Options

| Flag | Description |
|------|-------------|
| `-s` | Create symbolic links instead of hard links |
| `-f` | Force: remove existing target files |
| `-i` | Interactive: prompt before overwriting |
| `-n` | Don't follow symlinks on the target |
| `-v` | Verbose: print each link created |
| `-w` | Warn if symbolic link target does not exist |
| `-h` | Don't follow symlink if target is a symlink to a directory |
| `-F` | Remove existing target directory before linking |
| `-L` | Follow symlinks on the source |
| `-P` | Don't follow symlinks on the source (default for hard links) |

## Source Analysis

### Data Structures

```c
struct ln_options {
    bool force;                /* -f: remove existing targets */
    bool remove_dir;           /* -F: remove existing directories */
    bool no_target_follow;     /* -n/-h: don't follow target symlinks */
    bool interactive;          /* -i: prompt before replace */
    bool follow_source_symlink; /* -L: follow source symlinks */
    bool symbolic;             /* -s: create symlinks */
    bool verbose;              /* -v: print actions */
    bool warn_missing;         /* -w: warn on missing symlink target */
    int linkch;                /* Function: link or symlink */
};
```

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options, determine single vs. multi-target mode |
| `linkit()` | Create one link (core logic) |
| `remove_existing_target()` | Unlink or rmdir existing target |
| `samedirent()` | Check if source and target are the same file |
| `should_append_basename()` | Determine if target is a directory |
| `stat_parent_dir()` | Stat the parent directory of a path |
| `warn_missing_symlink_source()` | Check if symlink target exists |
| `prompt_replace()` | Interactive yes/no prompt |

### Core Linking Logic

```c
static int
linkit(const char *source, const char *target,
       const struct ln_options *opts)
{
    /* Check if target already exists */
    if (lstat(target, &sb) == 0) {
        /* Same file check */
        if (samedirent(source, target)) {
            warnx("%s and %s are the same", source, target);
            return 1;
        }

        /* Interactive prompt */
        if (opts->interactive && !prompt_replace(target))
            return 0;

        /* Remove existing target */
        if (opts->force || opts->interactive)
            remove_existing_target(target, opts);
    }

    /* Create the link */
    if (opts->symbolic) {
        if (symlink(source, target) < 0) {
            warn("symlink %s -> %s", target, source);
            return 1;
        }
    } else {
        if (link(source, target) < 0) {
            warn("link %s -> %s", target, source);
            return 1;
        }
    }

    /* Warn about dangling symlinks */
    if (opts->symbolic && opts->warn_missing)
        warn_missing_symlink_source(source, target);

    /* Verbose output */
    if (opts->verbose)
        printf("%s -> %s\n", target, source);

    return 0;
}
```

### Sameness Detection

```c
static int
samedirent(const char *source, const char *target)
{
    struct stat sb_src, sb_tgt;

    if (stat(source, &sb_src) < 0)
        return 0;
    if (stat(target, &sb_tgt) < 0)
        return 0;

    return (sb_src.st_dev == sb_tgt.st_dev &&
            sb_src.st_ino == sb_tgt.st_ino);
}
```

### Target Resolution

When the target is a directory, the source basename is appended:

```c
static int
should_append_basename(const char *target,
                       const struct ln_options *opts)
{
    struct stat sb;
    int (*statfn)(const char *, struct stat *);

    /* -n/-h: use lstat to not follow target symlinks */
    statfn = opts->no_target_follow ? lstat : stat;

    if (statfn(target, &sb) == 0 && S_ISDIR(sb.st_mode))
        return 1;
    return 0;
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `link(2)` | Create hard link |
| `symlink(2)` | Create symbolic link |
| `lstat(2)` | Stat without following symlinks |
| `stat(2)` | Stat following symlinks |
| `unlink(2)` | Remove existing target |
| `rmdir(2)` | Remove existing target directory (`-F`) |
| `readlink(2)` | Resolve symlink for display |

## Examples

```sh
# Hard link
ln file1.txt file2.txt

# Symbolic link
ln -s /usr/local/bin/python3 /usr/bin/python

# Force overwrite
ln -sf new_target link_name

# Verbose with warning
ln -svw /opt/myapp/bin/app /usr/local/bin/app

# Multiple files into directory
ln -s file1 file2 file3 /tmp/links/
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | All links created successfully |
| 1    | Error creating one or more links |
