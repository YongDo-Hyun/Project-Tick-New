# cp — Copy Files and Directories

## Overview

`cp` copies files and directory trees. It supports recursive copying, archive
mode (preserving metadata), symlink handling policies, sparse file detection,
and interactive/forced overwrite modes.

**Source**: `cp/cp.c`, `cp/utils.c`, `cp/extern.h`, `cp/fts.c`, `cp/fts.h`
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
cp [-HLPRafilnpsvx] [--sort] [-N mode] source_file target_file
cp [-HLPRafilnpsvx] [--sort] [-N mode] source_file ... target_directory
```

## Options

| Flag | Description |
|------|-------------|
| `-R` / `-r` | Recursive: copy directories and their contents |
| `-a` | Archive mode: equivalent to `-R -P -p` |
| `-f` | Force: remove existing target before copying |
| `-i` | Interactive: prompt before overwriting |
| `-l` | Create hard links instead of copying |
| `-n` | No-clobber: do not overwrite existing files |
| `-p` | Preserve: maintain mode, ownership, timestamps |
| `-s` | Create symbolic links instead of copying |
| `-v` | Verbose: print each file as it is copied |
| `-x` | One-filesystem: do not cross mount points |
| `-H` | Follow symlinks on command line (with `-R`) |
| `-L` | Follow all symbolic links (with `-R`) |
| `-P` | Do not follow symbolic links (default with `-R`) |
| `--sort` | Sort entries numerically during recursive copy |
| `-N mode` | Apply negated permissions to regular files |

## Source Analysis

### cp.c — Main Logic

#### Key Data Structures

```c
typedef struct {
    char *p_end;           /* Pointer to NULL at end of path */
    char *target_end;      /* Pointer to end of target base */
    char p_path[PATH_MAX]; /* Current target path buffer */
    int  p_fd;             /* Directory file descriptor */
} PATH_T;

struct options {
    bool recursive;
    bool force;
    bool interactive;
    bool no_clobber;
    bool preserve;
    bool hard_link;
    bool symbolic_link;
    bool verbose;
    bool one_filesystem;
    /* ... */
};
```

#### Key Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options, stat destination, determine copy mode |
| `copy()` | Main recursive copy driver using FTS traversal |
| `ftscmp()` | qsort comparator for `--sort` numeric ordering |
| `local_strlcpy()` | Portability wrapper for `strlcpy` |
| `local_asprintf()` | Portability wrapper for `asprintf` |

#### Copy Mode Detection

`cp` determines the operation mode from the arguments:

```c
/* Three cases:
 * 1. cp file1 file2           → file-to-file copy
 * 2. cp file1 file2 dir/      → files into directory
 * 3. cp -R dir1 dir2          → directory to new directory
 */
if (stat(target, &sb) == 0 && S_ISDIR(sb.st_mode))
    type = DIR_TO_DIR;
else if (argc == 2)
    type = FILE_TO_FILE;
else
    usage();  /* Multiple sources require directory target */
```

### utils.c — Copy Engine

#### Adaptive Buffer Sizing

Like `cat`, `cp` adapts its I/O buffer to available memory:

```c
#define PHYSPAGES_THRESHOLD (32*1024)
#define BUFSIZE_MAX         (2*1024*1024)
#define BUFSIZE_SMALL       (MAXPHYS)        /* 128 KB */

static ssize_t
copy_fallback(int from_fd, int to_fd)
{
    if (buf == NULL) {
        if (sysconf(_SC_PHYS_PAGES) > PHYSPAGES_THRESHOLD)
            bufsize = MIN(BUFSIZE_MAX, MAXPHYS * 8);
        else
            bufsize = BUFSIZE_SMALL;
        buf = malloc(bufsize);
    }
    /* read/write loop */
}
```

#### Key Functions in utils.c

| Function | Purpose |
|----------|---------|
| `copy_fallback()` | Buffer-based file copy with adaptive sizing |
| `copy_file()` | Copy regular file, potentially using `copy_file_range(2)` |
| `copy_link()` | Copy symbolic link (read target, create new symlink) |
| `copy_fifo()` | Copy FIFO via `mkfifo(2)` |
| `copy_special()` | Copy device nodes via `mknod(2)` |
| `setfile()` | Set timestamps, ownership, permissions on target |
| `preserve_fd_acls()` | Copy POSIX ACLs between file descriptors |

### FTS Traversal

`cp -R` uses an in-tree FTS (File Traversal Stream) implementation:

```c
FTS *ftsp;
FTSENT *curr;
int fts_options = FTS_NOCHDIR | FTS_PHYSICAL;

if (Lflag)
    fts_options &= ~FTS_PHYSICAL;
    fts_options |= FTS_LOGICAL;

ftsp = fts_open(argv, fts_options, NULL);
while ((curr = fts_read(ftsp)) != NULL) {
    switch (curr->fts_info) {
    case FTS_D:     /* Directory pre-visit */
        mkdir(target_path, curr->fts_statp->st_mode);
        break;
    case FTS_F:     /* Regular file */
        copy_file(curr->fts_path, target_path);
        break;
    case FTS_SL:    /* Symbolic link */
        copy_link(curr->fts_path, target_path);
        break;
    case FTS_DP:    /* Directory post-visit */
        setfile(curr->fts_statp, target_path);
        break;
    }
}
```

### Symlink Handling Modes

| Mode | Flag | Behavior |
|------|------|----------|
| Physical | `-P` (default) | Copy symlinks as symlinks |
| Command-line follow | `-H` | Follow symlinks named on command line |
| Logical | `-L` | Follow all symlinks, copy targets |

### Archive Mode

The `-a` flag combines three flags for complete archival:

```sh
cp -a source/ dest/
# Equivalent to:
cp -R -P -p source/ dest/
```

- `-R` — Recursive copy
- `-P` — Don't follow symlinks (preserve them as-is)
- `-p` — Preserve timestamps, ownership, and permissions

### Metadata Preservation (`-p`)

When `-p` is specified, `cp` preserves:

| Metadata | System Call |
|----------|-------------|
| Access time | `utimensat(2)` |
| Modification time | `utimensat(2)` |
| File mode | `fchmod(2)` / `chmod(2)` |
| Owner/group | `fchown(2)` / `lchown(2)` |
| ACLs | `acl_get_fd()` / `acl_set_fd()` (if available) |

### Cycle Detection

During recursive copy, `cp` tracks visited directories by `(dev, ino)`
pairs to detect filesystem cycles created by symlinks or bind mounts:

```c
/* If we've already visited this inode on this device, skip it */
if (cycle_check(curr->fts_statp->st_dev, curr->fts_statp->st_ino)) {
    warnx("%s: directory causes a cycle", curr->fts_path);
    fts_set(ftsp, curr, FTS_SKIP);
    continue;
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `open(2)` | Open source and target files |
| `read(2)` / `write(2)` | Buffer-based data copy |
| `copy_file_range(2)` | Zero-copy in-kernel transfer |
| `mkdir(2)` | Create target directories |
| `mkfifo(2)` | Create FIFO copies |
| `mknod(2)` | Create device node copies |
| `symlink(2)` | Create symbolic links |
| `link(2)` | Create hard links (with `-l`) |
| `readlink(2)` | Read symlink target |
| `fchmod(2)` | Set permissions on target |
| `fchown(2)` | Set ownership on target |
| `utimensat(2)` | Set timestamps on target |
| `fstat(2)` | Check file type and metadata |

## Examples

```sh
# Simple file copy
cp source.txt dest.txt

# Recursive directory copy
cp -R src/ dest/

# Archive mode (preserve everything)
cp -a project/ backup/project/

# Interactive overwrite
cp -i newfile.txt existing.txt

# Create hard links instead of copies
cp -l large_file.dat link_to_large_file.dat

# Don't cross filesystem boundaries
cp -Rx /home/user/ /backup/home/user/

# Verbose recursive copy
cp -Rv config/ /etc/myapp/
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | All files copied successfully |
| 1    | Error copying one or more files |

## Differences from GNU cp

- No `--reflink` option for CoW copies
- No `--sparse=auto/always/never` (sparse handling is automatic)
- `--sort` flag for sorted recursive output (not in GNU)
- `-N` flag for negated permissions (not in GNU)
- Uses in-tree FTS instead of gnulib
- No SELinux context preservation (use `--preserve=context` in GNU)
