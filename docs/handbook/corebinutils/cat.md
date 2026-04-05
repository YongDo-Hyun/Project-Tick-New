# cat — Concatenate and Display Files

## Overview

`cat` reads files sequentially and writes their contents to standard output.
It supports line numbering, non-printable character visualization, blank
line squeezing, and efficient in-kernel copying.

**Source**: `cat/cat.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
cat [-belnstuv] [file ...]
```

## Options

| Flag | Long Form | Description |
|------|-----------|-------------|
| `-b` | — | Number non-blank output lines (starting at 1) |
| `-e` | — | Display `$` at end of each line (implies `-v`) |
| `-l` | — | Set exclusive advisory lock on stdout via `flock(2)` |
| `-n` | — | Number all output lines |
| `-s` | — | Squeeze multiple adjacent blank lines into one |
| `-t` | — | Display TAB as `^I` (implies `-v`) |
| `-u` | — | Disable output buffering (write immediately) |
| `-v` | — | Visualize non-printing characters using `^X` and `M-X` notation |

## Source Analysis

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Option parsing via `getopt(3)`, dispatch to `scanfiles()` |
| `usage()` | Print usage message and exit |
| `scanfiles()` | Iterate over file arguments, handle `-` as stdin |
| `cook_cat()` | Process input with formatting (numbering, visualization, squeezing) |
| `raw_cat()` | Fast buffer-based copy without formatting |
| `in_kernel_copy()` | Zero-copy via `copy_file_range(2)` syscall |
| `init_casper()` | BSD Capsicum sandbox init (disabled on Linux) |
| `init_casper_net()` | BSD Casper network service (disabled on Linux) |
| `udom_open()` | Unix domain socket support (disabled on Linux) |

### Option Processing

```c
while ((ch = getopt(argc, argv, "belnstuv")) != -1)
    switch (ch) {
    case 'b': bflag = nflag = 1; break;  /* implies -n */
    case 'e': eflag = vflag = 1; break;  /* implies -v */
    case 'l': lflag = 1; break;
    case 'n': nflag = 1; break;
    case 's': sflag = 1; break;
    case 't': tflag = vflag = 1; break;  /* implies -v */
    case 'u': setbuf(stdout, NULL); break;
    case 'v': vflag = 1; break;
    default:  usage();
    }
```

### I/O Strategy: Three Modes

`cat` selects among three output strategies based on which flags are active:

1. **`in_kernel_copy()`** — When no formatting flags are set and the output
   supports `copy_file_range(2)`, data moves directly between file
   descriptors inside the kernel, never entering user space.

2. **`raw_cat()`** — When no formatting is needed but `copy_file_range` is
   unavailable (e.g., stdin is a pipe). Uses an adaptive read buffer.

3. **`cook_cat()`** — When any formatting flag (`-b`, `-e`, `-n`, `-s`,
   `-t`, `-v`) is active. Processes each character individually.

### Adaptive Buffer Sizing

`raw_cat()` dynamically sizes its read buffer based on available physical
memory:

```c
#define PHYSPAGES_THRESHOLD (32*1024)
#define BUFSIZE_MAX         (2*1024*1024)   /* 2 MB */
#define BUFSIZE_SMALL       (128*1024)      /* 128 KB */

if (sysconf(_SC_PHYS_PAGES) > PHYSPAGES_THRESHOLD)
    bsize = MIN(BUFSIZE_MAX, MAXPHYS * 8);
else
    bsize = BUFSIZE_SMALL;
```

On systems with more than 128 MB of RAM (`32K × 4K pages`), cat uses up
to 2 MB buffers. On constrained systems, it falls back to 128 KB.

### In-Kernel Copy

When possible, `cat` uses the Linux `copy_file_range(2)` syscall for
zero-copy I/O:

```c
static int
in_kernel_copy(int from_fd, int to_fd)
{
    ssize_t ret;

    do {
        ret = copy_file_range(from_fd, NULL, to_fd, NULL, SSIZE_MAX, 0);
    } while (ret > 0);

    return (ret == 0) ? 0 : -1;
}
```

This avoids two context switches per block (kernel→user for read,
user→kernel for write) and can be significantly faster for large files.

### Character Visualization

When `-v` is active, `cook_cat()` renders non-printable characters:

| Character Range | Rendering | Example |
|----------------|-----------|---------|
| `0x00–0x1F`    | `^@` to `^_` | `^C` for ETX |
| `0x7F`         | `^?`      | DEL character |
| `0x80–0x9F`    | `M-^@` to `M-^_` | Meta-control |
| `0xA0–0xFE`    | `M- ` to `M-~` | Meta-printable |
| `0xFF`         | `M-^?`    | Meta-DEL |
| TAB (`0x09`)   | `^I` (with `-t`) or literal | |
| Newline        | `$\n` (with `-e`) or `\n` | |

### Locale Support

`cat` calls `setlocale(LC_CTYPE, "")` for wide character handling. In
multibyte locales, the `-v` flag considers locale-specific printability
via `iswprint(3)`. In the C locale, only ASCII printable characters
pass through unmodified.

### Lock Mode

The `-l` flag acquires an exclusive advisory lock on stdout before
writing:

```c
if (lflag)
    flock(STDOUT_FILENO, LOCK_EX);
```

This prevents interleaved output when multiple `cat` processes write to
the same file simultaneously. The lock is held for the entire duration
of the program.

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `open(2)` | Open input files |
| `read(2)` | Read file data into buffer |
| `write(2)` | Write processed data to stdout |
| `copy_file_range(2)` | Zero-copy kernel-to-kernel transfer |
| `flock(2)` | Advisory locking (with `-l`) |
| `fstat(2)` | Get file type for I/O strategy selection |
| `sysconf(3)` | Query physical page count for buffer sizing |

## BSD Features Disabled on Linux

Several BSD-specific features are compiled out on Linux:

- **Capsicum sandbox** (`cap_enter`, `cap_rights_limit`): The
  `init_casper()` function is a no-op stub on Linux.
- **Unix domain socket reading** (`udom_open()`): BSD `cat` can read
  from Unix sockets via `connect(2)`. Disabled on Linux.
- **`O_RESOLVE_BENEATH`**: BSD sandbox path resolution flag. Defined to 0
  on Linux.

## Examples

```sh
# Concatenate files
cat file1.txt file2.txt > combined.txt

# Number non-blank lines
cat -b source.c

# Show invisible characters
cat -vet binary_file

# Squeeze blank lines and number
cat -sn logfile.txt

# Lock stdout for atomic output
cat -l data.csv >> shared_output.csv
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | All files read successfully |
| 1    | Error opening or reading a file |

## Edge Cases

- Reading from stdin: When no files are specified or `-` is given, cat
  reads from standard input.
- Empty files: Produce no output but are not errors.
- Binary files: Processed byte-by-byte; `-v` makes them viewable.
- Named pipes and devices: `raw_cat()` handles them with buffered reads.
  `copy_file_range` is not attempted on non-regular files.
