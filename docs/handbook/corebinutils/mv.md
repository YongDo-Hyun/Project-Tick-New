# mv — Move (Rename) Files

## Overview

`mv` moves or renames files and directories. When the source and target
are on the same filesystem, it uses `rename(2)`. When they are on
different filesystems, it performs a copy-and-remove fallback with
extended attribute preservation.

**Source**: `mv/mv.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
mv [-finv] source target
mv [-finv] source ... directory
```

## Options

| Flag | Description |
|------|-------------|
| `-f` | Force: do not prompt before overwriting |
| `-i` | Interactive: prompt before overwriting |
| `-n` | No clobber: do not overwrite existing files |
| `-v` | Verbose: print each file as it is moved |

## Source Analysis

### Data Structures

```c
struct mv_options {
    bool force;               /* -f: overwrite without asking */
    bool interactive;         /* -i: prompt before overwrite */
    bool no_clobber;          /* -n: never overwrite */
    bool no_target_dir_follow; /* Don't follow target symlinks */
    bool verbose;             /* -v: display moves */
};

struct move_target {
    char *path;
    struct stat sb;
    bool is_directory;
};
```

### Constants

```c
#define MV_EXIT_ERROR  1
#define MV_EXIT_USAGE  2

#define COPY_BUFFER_MIN  (128 * 1024)   /* 128 KB */
#define COPY_BUFFER_MAX  (2 * 1024 * 1024)  /* 2 MB */
```

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options, determine single vs. multi-target |
| `handle_single_move()` | Move one source to one target |
| `apply_existing_target_policy()` | Handle `-f`, `-i`, `-n` logic |
| `copy_move_fallback()` | Cross-device copy+remove |
| `copy_file_data()` | Buffer-based data copy |
| `copy_file_xattrs()` | Preserve extended attributes |
| `copy_directory_tree()` | Recursive directory copy |
| `apply_path_metadata()` | Set ownership, permissions, timestamps |
| `remove_source_tree()` | Remove original after copy |

### Core Move Logic

```c
static int
handle_single_move(const char *source, const char *target,
                   const struct mv_options *opts)
{
    /* Check for self-move */
    struct stat src_sb, tgt_sb;
    if (stat(source, &src_sb) < 0)
        return MV_EXIT_ERROR;

    /* Handle existing target */
    if (lstat(target, &tgt_sb) == 0) {
        /* Same file? (device + inode) */
        if (src_sb.st_dev == tgt_sb.st_dev &&
            src_sb.st_ino == tgt_sb.st_ino) {
            warnx("'%s' and '%s' are the same file", source, target);
            return MV_EXIT_ERROR;
        }

        /* Apply -f/-i/-n policy */
        int policy = apply_existing_target_policy(target, &tgt_sb, opts);
        if (policy != 0)
            return policy;
    }

    /* Try rename(2) first — fast path */
    if (rename(source, target) == 0) {
        if (opts->verbose)
            printf("'%s' -> '%s'\n", source, target);
        return 0;
    }

    /* Cross-device: copy then remove */
    if (errno == EXDEV)
        return copy_move_fallback(source, target, &src_sb, opts);

    warn("rename '%s' to '%s'", source, target);
    return MV_EXIT_ERROR;
}
```

### Cross-Device Copy Fallback

When `rename(2)` fails with `EXDEV` (different filesystems):

```c
static int
copy_move_fallback(const char *source, const char *target,
                   const struct stat *src_sb,
                   const struct mv_options *opts)
{
    if (S_ISDIR(src_sb->st_mode)) {
        /* Recursive directory copy */
        if (copy_directory_tree(source, target) != 0)
            return MV_EXIT_ERROR;
    } else {
        /* Regular file copy */
        if (copy_file_data(source, target) != 0)
            return MV_EXIT_ERROR;
    }

    /* Preserve metadata */
    apply_path_metadata(target, src_sb);

    /* Preserve extended attributes */
    copy_file_xattrs(source, target);

    /* Remove original */
    remove_source_tree(source, src_sb);

    if (opts->verbose)
        printf("'%s' -> '%s'\n", source, target);

    return 0;
}
```

### Adaptive Buffer Sizing

```c
static int
copy_file_data(const char *source, const char *target)
{
    /* Allocate buffer based on available memory */
    size_t bufsize = COPY_BUFFER_MAX;
    char *buf = NULL;

    while (bufsize >= COPY_BUFFER_MIN) {
        buf = malloc(bufsize);
        if (buf) break;
        bufsize /= 2;
    }

    int src_fd = open(source, O_RDONLY);
    int tgt_fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    ssize_t n;
    while ((n = read(src_fd, buf, bufsize)) > 0) {
        if (write_all(tgt_fd, buf, n) < 0) {
            warn("write '%s'", target);
            return -1;
        }
    }

    free(buf);
    close(src_fd);
    close(tgt_fd);
    return 0;
}
```

### Extended Attribute Preservation

```c
#include <sys/xattr.h>

static void
copy_file_xattrs(const char *source, const char *target)
{
    ssize_t list_len = listxattr(source, NULL, 0);
    if (list_len <= 0)
        return;

    char *list = malloc(list_len);
    listxattr(source, list, list_len);

    for (char *name = list; name < list + list_len;
         name += strlen(name) + 1) {
        ssize_t val_len = getxattr(source, name, NULL, 0);
        if (val_len < 0) continue;

        char *val = malloc(val_len);
        getxattr(source, name, val, val_len);
        setxattr(target, name, val, val_len, 0);
        free(val);
    }

    free(list);
}
```

### Metadata Preservation

```c
static void
apply_path_metadata(const char *target, const struct stat *sb)
{
    /* Ownership */
    chown(target, sb->st_uid, sb->st_gid);

    /* Permissions */
    chmod(target, sb->st_mode);

    /* Timestamps */
    struct timespec times[2] = {
        sb->st_atim,   /* Access time */
        sb->st_mtim,   /* Modification time */
    };
    utimensat(AT_FDCWD, target, times, 0);
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `rename(2)` | Same-device move (fast path) |
| `read(2)` / `write(2)` | Cross-device data copy |
| `stat(2)` / `lstat(2)` | File metadata |
| `chown(2)` | Preserve ownership |
| `chmod(2)` | Preserve permissions |
| `utimensat(2)` | Preserve timestamps |
| `listxattr(2)` | List extended attributes |
| `getxattr(2)` / `setxattr(2)` | Copy extended attributes |
| `unlink(2)` / `rmdir(2)` | Remove source after copy |

## Examples

```sh
# Rename a file
mv old.txt new.txt

# Move into directory
mv file.txt /tmp/

# Interactive mode
mv -i important.txt /backup/

# No clobber
mv -n *.txt /archive/

# Verbose
mv -v file1 file2 file3 /dest/
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | All moves successful |
| 1    | Error during move |
| 2    | Usage error |

## Differences from GNU mv

- No `--backup` / `-b` option
- No `--suffix` / `-S`
- No `--target-directory` / `-t`
- No `--update` / `-u`
- Simpler cross-device fallback without sparse file optimization
