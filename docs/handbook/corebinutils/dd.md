# dd — Data Duplicator

## Overview

`dd` copies and optionally converts data between files or devices. It operates
at the block level with configurable input/output block sizes, supports
ASCII/EBCDIC conversion, case conversion, byte swapping, sparse output,
speed throttling, and real-time progress reporting.

**Source**: `dd/dd.c`, `dd/dd.h`, `dd/extern.h`, `dd/args.c`, `dd/conv.c`,
`dd/conv_tab.c`, `dd/gen.c`, `dd/misc.c`, `dd/position.c`
**Origin**: BSD 4.4, Keith Muller / Lance Visser
**License**: BSD-3-Clause

## Synopsis

```
dd [operand=value ...]
```

## Operands

`dd` uses a unique JCL-style syntax (not `getopt`):

| Operand | Description | Default |
|---------|-------------|---------|
| `if=file` | Input file | stdin |
| `of=file` | Output file | stdout |
| `bs=n` | Block size (sets both ibs and obs) | 512 |
| `ibs=n` | Input block size | 512 |
| `obs=n` | Output block size | 512 |
| `cbs=n` | Conversion block size | — |
| `count=n` | Number of input blocks to copy | All |
| `skip=n` / `iseek=n` | Skip n input blocks | 0 |
| `seek=n` / `oseek=n` | Seek n output blocks | 0 |
| `files=n` | Copy n input files (tape only) | 1 |
| `fillchar=c` | Fill character for sync padding | NUL/space |
| `speed=n` | Maximum bytes per second | Unlimited |
| `status=value` | Progress reporting mode | — |

### Size Suffixes

Numeric values accept multiplier suffixes:

| Suffix | Multiplier |
|--------|-----------|
| `b` | 512 |
| `k` | 1024 |
| `m` | 1024² (1,048,576) |
| `g` | 1024³ (1,073,741,824) |
| `t` | 1024⁴ |
| `w` | `sizeof(int)` |
| `x` | Multiplication (e.g., `2x512` = 1024) |

### conv= Options

| Conversion | Description |
|------------|-------------|
| `ascii` | EBCDIC to ASCII |
| `ebcdic` | ASCII to EBCDIC |
| `ibm` | ASCII to IBM EBCDIC |
| `block` | Newline-terminated to fixed-length records |
| `unblock` | Fixed-length records to newline-terminated |
| `lcase` | Convert to lowercase |
| `ucase` | Convert to uppercase |
| `swab` | Swap every pair of bytes |
| `noerror` | Continue after read errors |
| `notrunc` | Don't truncate output file |
| `sync` | Pad input blocks to ibs with NULs/spaces |
| `sparse` | Seek over zero-filled output blocks |
| `fsync` | Physically write output data and metadata |
| `fdatasync` | Physically write output data |
| `pareven` | Set even parity on output |
| `parodd` | Set odd parity on output |
| `parnone` | Strip parity from input |
| `parset` | Set parity bit on output |

### iflag= and oflag= Options

| Flag | Description |
|------|-------------|
| `direct` | Use `O_DIRECT` for direct I/O |
| `fullblock` | Accumulate full input blocks |

### status= Values

| Value | Description |
|-------|-------------|
| `noxfer` | Suppress transfer statistics |
| `none` | Suppress everything |
| `progress` | Print periodic progress via SIGALRM |

## Source Architecture

### File Responsibilities

| File | Purpose |
|------|---------|
| `dd.c` | Main control flow: `main()`, `setup()`, `dd_in()`, `dd_close()` |
| `dd.h` | Shared types: `IO`, `STAT`, conversion flags (40+ bit flags) |
| `extern.h` | External function declarations and global variable exports |
| `args.c` | JCL argument parser: `jcl()`, operand table, size parsing |
| `conv.c` | Conversion functions: `def()`, `block()`, `unblock()` |
| `conv_tab.c` | ASCII/EBCDIC translation tables (256-byte arrays) |
| `gen.c` | Signal handling: `prepare_io()`, `before_io()`, `after_io()` |
| `misc.c` | Summary output, progress reporting, timing |
| `position.c` | Input/output positioning: `pos_in()`, `pos_out()` |

### Key Data Structures

#### IO Structure (I/O Stream State)

```c
typedef struct {
    u_char  *db;        /* Buffer address */
    u_char  *dbp;       /* Current buffer I/O pointer */
    ssize_t  dbcnt;     /* Current buffer byte count */
    ssize_t  dbrcnt;    /* Last read byte count */
    ssize_t  dbsz;      /* Block size */
    u_int    flags;     /* ISCHR | ISPIPE | ISTAPE | ISSEEK | NOREAD | ISTRUNC */
    const char *name;   /* Filename */
    int      fd;        /* File descriptor */
    off_t    offset;    /* Blocks to skip */
    off_t    seek_offset; /* Sparse output offset */
} IO;
```

Device type flags:

| Flag | Value | Meaning |
|------|-------|---------|
| `ISCHR` | 0x01 | Character device |
| `ISPIPE` | 0x02 | Pipe or socket |
| `ISTAPE` | 0x04 | Tape device |
| `ISSEEK` | 0x08 | Seekable |
| `NOREAD` | 0x10 | Write-only (output opened without read) |
| `ISTRUNC` | 0x20 | Truncatable |

#### STAT Structure (Statistics)

```c
typedef struct {
    uintmax_t in_full;   /* Full input blocks transferred */
    uintmax_t in_part;   /* Partial input blocks */
    uintmax_t out_full;  /* Full output blocks */
    uintmax_t out_part;  /* Partial output blocks */
    uintmax_t trunc;     /* Truncated records */
    uintmax_t swab;      /* Odd-length swab blocks */
    uintmax_t bytes;     /* Total bytes written */
    struct timespec start; /* Start timestamp */
} STAT;
```

#### Conversion Flags

The `ddflags` global is a 64-bit bitmask with 37 defined flags:

```c
#define C_ASCII       0x0000000000000001ULL
#define C_BLOCK       0x0000000000000002ULL
#define C_BS          0x0000000000000004ULL
/* ... 34 more flags ... */
#define C_IDIRECT     0x0000000800000000ULL
#define C_ODIRECT     0x0000001000000000ULL
```

### Argument Parsing (args.c)

`dd` uses its own JCL-style parser instead of `getopt`:

```c
static const struct arg {
    const char *name;
    void (*f)(char *);
    uint64_t set, noset;
} args[] = {
    { "bs",       f_bs,       C_BS,    C_BS|C_IBS|C_OBS|C_OSYNC },
    { "cbs",      f_cbs,      C_CBS,   C_CBS },
    { "conv",     f_conv,     0,       0 },
    { "count",    f_count,    C_COUNT, C_COUNT },
    { "files",    f_files,    C_FILES, C_FILES },
    { "fillchar", f_fillchar, C_FILL,  C_FILL },
    { "ibs",      f_ibs,      C_IBS,   C_BS|C_IBS },
    { "if",       f_if,       C_IF,    C_IF },
    { "iflag",    f_iflag,    0,       0 },
    { "obs",      f_obs,      C_OBS,   C_BS|C_OBS },
    { "of",       f_of,       C_OF,    C_OF },
    { "oflag",    f_oflag,    0,       0 },
    { "seek",     f_seek,     C_SEEK,  C_SEEK },
    { "skip",     f_skip,     C_SKIP,  C_SKIP },
    { "speed",    f_speed,    0,       0 },
    { "status",   f_status,   C_STATUS,C_STATUS },
};
```

Arguments are looked up via `bsearch()` in the sorted table.

The `noset` field prevents conflicting options: e.g., `bs=` sets
`C_BS|C_IBS|C_OBS|C_OSYNC` and forbids re-specifying any of those.

### Conversion Functions (conv.c)

Three conversion modes:

#### `def()` — Default (No Conversion)

```c
void def(void)
{
    if ((t = ctab) != NULL)
        for (inp = in.dbp - (cnt = in.dbrcnt); cnt--; ++inp)
            *inp = t[*inp];

    out.dbp = in.dbp;
    out.dbcnt = in.dbcnt;

    if (in.dbcnt >= out.dbsz)
        dd_out(0);
}
```

Simple buffer pass-through with optional character table translation.

#### `block()` — Variable → Fixed Length

Converts newline-terminated records to fixed-length records padded with
spaces to `cbs` bytes. Used for ASCII-to-EBCDIC record conversion.

#### `unblock()` — Fixed → Variable Length

Converts fixed-length records back to newline-terminated format by
stripping trailing spaces and appending newlines.

### Signal Handling (gen.c)

`dd` handles several signals:

| Signal | Handler | Purpose |
|--------|---------|---------|
| SIGINFO/SIGUSR1 | `siginfo_handler()` | Print transfer summary |
| SIGALRM | `sigalarm_handler()` | Periodic progress (with `status=progress`) |
| SIGINT/SIGTERM | Default + atexit | Print summary before exit |

The `prepare_io()`, `before_io()`, `after_io()` functions manage signal
masking during I/O operations to prevent interruption during critical
sections.

```c
volatile sig_atomic_t need_summary;   /* Set by SIGINFO */
volatile sig_atomic_t need_progress;  /* Set by SIGALRM */
volatile sig_atomic_t kill_signal;    /* Set by termination signals */
```

### Progress Reporting (misc.c)

```c
void summary(void)
{
    /* Print: "X+Y records in\nA+B records out\nN bytes transferred in T secs" */
    double elapsed = secs_elapsed();
    /* Print human-readable transfer rate */
}
```

The `format_scaled()` helper renders byte counts in human-readable form
(kB, MB, GB) using configurable base (1000 or 1024).

### Buffer Allocation

Direct I/O requires page-aligned buffers:

```c
static void *
alloc_io_buffer(size_t size)
{
    if ((ddflags & (C_IDIRECT | C_ODIRECT)) == 0)
        return malloc(size);

    size_t alignment = sysconf(_SC_PAGESIZE);
    if (alignment == 0 || alignment == (size_t)-1)
        alignment = 4096;
    void *buf;
    posix_memalign(&buf, alignment, size);
    return buf;
}
```

### Sparse Output

With `conv=sparse`, `dd` uses `lseek(2)` to skip over blocks of zeros
instead of writing them:

```c
#define BISZERO(p, s) ((s) > 0 && *((const char *)p) == 0 && \
    !memcmp((const void *)(p), (const void *)((const char *)p + 1), (s) - 1))
```

### Setup and I/O

The `setup()` function in `dd.c` handles:

1. Opening input/output files with appropriate flags
2. Detecting device types (`getfdtype()`)
3. Allocating I/O buffers
4. Setting up character conversion tables (parity, case)
5. Positioning input/output streams
6. Truncating output if needed

```c
static void setup(void)
{
    /* Open input */
    if (in.name == NULL) {
        in.name = "stdin";
        in.fd = STDIN_FILENO;
    } else {
        iflags = (ddflags & C_IDIRECT) ? O_DIRECT : 0;
        in.fd = open(in.name, O_RDONLY | iflags, 0);
    }

    /* Open output */
    oflags = O_CREAT;
    if (!(ddflags & (C_SEEK | C_NOTRUNC)))
        oflags |= O_TRUNC;
    if (ddflags & C_OFSYNC)
        oflags |= O_SYNC;
    if (ddflags & C_ODIRECT)
        oflags |= O_DIRECT;
    out.fd = open(out.name, O_RDWR | oflags, DEFFILEMODE);

    /* Allocate buffers */
    if (!(ddflags & (C_BLOCK | C_UNBLOCK))) {
        in.db = alloc_io_buffer(out.dbsz + in.dbsz - 1);
        out.db = in.db;  /* Single shared buffer */
    } else {
        in.db = alloc_io_buffer(MAX(in.dbsz, cbsz) + cbsz);
        out.db = alloc_io_buffer(out.dbsz + cbsz);
    }
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `open(2)` | Open input/output files |
| `read(2)` | Read input blocks |
| `write(2)` | Write output blocks |
| `lseek(2)` | Position streams, sparse output |
| `ftruncate(2)` | Truncate output file |
| `close(2)` | Close file descriptors |
| `ioctl(2)` | Tape device queries (`MTIOCGET`) |
| `sigaction(2)` | Install signal handlers |
| `setitimer(2)` | Periodic SIGALRM for progress |
| `clock_gettime(2)` | Elapsed time calculation |
| `posix_memalign(3)` | Page-aligned buffers for direct I/O |
| `sysconf(3)` | Get page size |

## Examples

```sh
# Copy a disk image
dd if=/dev/sda of=disk.img bs=4M status=progress

# Write an image to a device
dd if=image.iso of=/dev/sdb bs=4M conv=fsync

# Create a 1GB sparse file
dd if=/dev/zero of=sparse.img bs=1 count=0 seek=1G

# Convert ASCII to uppercase
dd if=input.txt of=output.txt conv=ucase

# Copy with direct I/O
dd if=data.bin of=data2.bin bs=4k iflag=direct oflag=direct

# Network transfer (with throttling)
dd if=large_file.tar bs=1M speed=10M | ssh remote 'dd of=large_file.tar'

# Skip first 100 blocks of input
dd if=tape.raw of=data.bin skip=100 bs=512

# Show progress with SIGUSR1
dd if=/dev/sda of=backup.img bs=1M &
kill -USR1 $!
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Error (open, read, write, or conversion failure) |

## Summary Output Format

```
X+Y records in
A+B records out
N bytes (H) transferred in T.TTT secs (R/s)
```

Where:
- `X` = full input blocks, `Y` = partial input blocks
- `A` = full output blocks, `B` = partial output blocks
- `N` = total bytes, `H` = human-readable size
- `T` = elapsed seconds, `R` = transfer rate
