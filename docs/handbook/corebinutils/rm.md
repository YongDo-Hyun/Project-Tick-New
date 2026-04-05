# rm — Remove Files and Directories

## Overview

`rm` removes files and directories. It supports recursive removal,
interactive prompting, forced deletion, and protects against removal
of `/`, `.`, and `..`. Directory traversal uses `openat(2)` and
`fdopendir(3)` for safe recursive descent.

**Source**: `rm/rm.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
rm [-dfiIPRrvWx] file ...
```

## Options

| Flag | Description |
|------|-------------|
| `-d` | Remove empty directories (like `rmdir`) |
| `-f` | Force: no prompts, ignore nonexistent files |
| `-i` | Interactive: prompt for each file |
| `-I` | Prompt once before recursive removal or >3 files |
| `-P` | Overwrite before delete (BSD; not on Linux) |
| `-R` / `-r` | Recursive: remove directories and contents |
| `-v` | Verbose: print each file as removed |
| `-W` | Whiteout (BSD union fs; not on Linux) |
| `-x` | Stay on one filesystem |

## Source Analysis

### Data Structures

```c
struct options_t {
    bool force;         /* -f */
    bool interactive;   /* -i */
    bool prompt_once;   /* -I */
    bool recursive;     /* -R/-r */
    bool remove_empty;  /* -d */
    bool verbose;       /* -v */
    bool one_fs;        /* -x */
    bool stdin_tty;     /* Whether stdin is a TTY */
};
```

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options and dispatch |
| `remove_path()` | Remove a single top-level argument |
| `remove_simple_path()` | Remove non-directory file |
| `remove_path_at()` | Recursive removal at directory fd |
| `prompt_for_removal()` | Interactive prompt for single file |
| `prompt_for_directory_descent()` | Prompt before entering directory |
| `prompt_once()` | One-time batch prompt (`-I`) |
| `prompt_yesno()` | Read yes/no from terminal |
| `join_path()` | Path concatenation |
| `path_is_writable()` | Check write access |

### Safety Checks

```c
static int
remove_path(const char *path, const struct options_t *opts)
{
    /* Reject "/" */
    if (strcmp(path, "/") == 0) {
        warnx("\"/\" may not be removed");
        return 1;
    }

    /* Reject "." and ".." */
    const char *base = basename(path);
    if (strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
        warnx("\".\" and \"..\" may not be removed");
        return 1;
    }

    struct stat sb;
    if (lstat(path, &sb) < 0) {
        if (opts->force)
            return 0;  /* Silently ignore */
        warn("%s", path);
        return 1;
    }

    if (S_ISDIR(sb.st_mode) && opts->recursive)
        return remove_path_at(AT_FDCWD, path, &sb, opts);
    else
        return remove_simple_path(path, &sb, opts);
}
```

### Recursive Removal

```c
static int
remove_path_at(int dirfd, const char *path,
               const struct stat *sb,
               const struct options_t *opts)
{
    /* Prompt before descending */
    if (opts->interactive &&
        !prompt_for_directory_descent(path))
        return 0;

    /* Open directory safely */
    int fd = openat(dirfd, path,
                    O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0) {
        warn("cannot open '%s'", path);
        return 1;
    }

    /* One-filesystem check */
    if (opts->one_fs) {
        struct stat dir_sb;
        fstat(fd, &dir_sb);
        if (dir_sb.st_dev != sb->st_dev) {
            warnx("skipping '%s' (different filesystem)", path);
            close(fd);
            return 1;
        }
    }

    DIR *dp = fdopendir(fd);
    struct dirent *ent;
    int errors = 0;

    while ((ent = readdir(dp)) != NULL) {
        /* Skip . and .. */
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' ||
             (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
            continue;

        struct stat child_sb;
        if (fstatat(fd, ent->d_name, &child_sb,
                    AT_SYMLINK_NOFOLLOW) < 0) {
            warn("%s/%s", path, ent->d_name);
            errors = 1;
            continue;
        }

        if (S_ISDIR(child_sb.st_mode)) {
            /* Cycle detection: compare device/inode */
            errors |= remove_path_at(fd,
                          ent->d_name, &child_sb, opts);
        } else {
            /* Prompt and remove */
            if (!opts->force &&
                !prompt_for_removal(path, ent->d_name,
                                    &child_sb, opts))
                continue;
            if (unlinkat(fd, ent->d_name, 0) < 0) {
                warn("cannot remove '%s/%s'", path, ent->d_name);
                errors = 1;
            } else if (opts->verbose) {
                printf("removed '%s/%s'\n", path, ent->d_name);
            }
        }
    }
    closedir(dp);

    /* Remove the directory itself */
    if (unlinkat(dirfd, path, AT_REMOVEDIR) < 0) {
        warn("cannot remove '%s'", path);
        errors = 1;
    } else if (opts->verbose) {
        printf("removed directory '%s'\n", path);
    }

    return errors;
}
```

### Interactive Prompting

```c
static bool
prompt_for_removal(const char *dir, const char *name,
                   const struct stat *sb,
                   const struct options_t *opts)
{
    if (opts->force)
        return true;

    /* Always prompt in -i mode */
    if (opts->interactive) {
        fprintf(stderr, "remove %s '%s/%s'? ",
                filetype_name(sb->st_mode), dir, name);
        return prompt_yesno();
    }

    /* Prompt for non-writable files (unless -f) */
    if (!path_is_writable(sb) && opts->stdin_tty) {
        fprintf(stderr, "remove write-protected %s '%s/%s'? ",
                filetype_name(sb->st_mode), dir, name);
        return prompt_yesno();
    }

    return true;
}

static bool
prompt_yesno(void)
{
    char buf[128];
    if (fgets(buf, sizeof(buf), stdin) == NULL)
        return false;
    return (buf[0] == 'y' || buf[0] == 'Y');
}
```

### Batch Prompt (-I)

```c
static bool
prompt_once(int count, const char *first_path,
            const struct options_t *opts)
{
    if (!opts->prompt_once)
        return true;

    if (count > 3 || opts->recursive) {
        fprintf(stderr,
                "remove %d arguments%s? ",
                count,
                opts->recursive ? " recursively" : "");
        return prompt_yesno();
    }
    return true;
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `unlink(2)` | Remove file |
| `unlinkat(2)` | Remove file/directory relative to dirfd |
| `openat(2)` | Open directory for traversal |
| `fdopendir(3)` | DIR stream from file descriptor |
| `fstatat(2)` | Stat relative to dirfd |
| `lstat(2)` | Stat without following symlinks |
| `readdir(3)` | Read directory entries |
| `rmdir(2)` | Remove empty directory |

## Examples

```sh
# Remove a file
rm file.txt

# Force remove (no prompts)
rm -f *.o

# Recursive remove
rm -rf build/

# Interactive
rm -ri important_dir/

# Verbose
rm -rv old_directory/

# Prompt once
rm -I *.log

# Stay on one filesystem
rm -rx /mounted/dir/
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | All files removed successfully |
| 1    | Error removing one or more files |

## Differences from GNU rm

- No `--preserve-root` / `--no-preserve-root` (always refuses `/`)
- No `--one-file-system` long option (uses `-x` instead)
- No `--interactive=WHEN` (only `-i` and `-I`)
- `-P` (overwrite) is BSD-only and not functional on Linux
- `-W` (whiteout) is BSD-only
