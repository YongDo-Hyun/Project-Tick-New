# ps — Process Status

## Overview

`ps` displays information about active processes. This implementation
reads process data from the Linux `/proc` filesystem and presents it
through BSD-style format strings. It provides a custom `struct kinfo_proc`
that mirrors FreeBSD's interface while reading from Linux procfs.

**Source**: `ps/ps.c`, `ps/ps.h`, `ps/fmt.c`, `ps/keyword.c`,
`ps/print.c`, `ps/nlist.c`, `ps/extern.h`
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
ps [-AaCcdefHhjLlMmrSTuvwXxZ] [-D fmt] [-G gid[,gid...]]
   [-J jail] [-N system] [-O fmt] [-o fmt] [-p pid[,pid...]]
   [-t tty[,tty...]] [-U user[,user...]] [-g group[,group...]]
```

## Source Architecture

### File Responsibilities

| File | Purpose |
|------|---------|
| `ps.c` | Main program, option parsing, process collection |
| `ps.h` | Data structures, constants, STAILQ macros |
| `fmt.c` | Format string parsing and column management |
| `keyword.c` | Format keyword definitions and lookup table |
| `print.c` | Column value formatters (PID, user, CPU, etc.) |
| `nlist.c` | Name list support (noop on Linux) |
| `extern.h` | Cross-file function declarations |

### Key Data Structures

#### Process Information (Linux replacement for BSD kinfo_proc)

```c
struct kinfo_proc {
    pid_t   ki_pid;        /* Process ID */
    pid_t   ki_ppid;       /* Parent PID */
    pid_t   ki_pgid;       /* Process group ID */
    pid_t   ki_sid;        /* Session ID */
    uid_t   ki_uid;        /* Real UID */
    uid_t   ki_ruid;       /* Real UID (copy) */
    uid_t   ki_svuid;      /* Saved UID */
    gid_t   ki_rgid;       /* Real GID */
    gid_t   ki_svgid;      /* Saved GID */
    gid_t   ki_groups[KI_NGROUPS]; /* Supplementary groups */
    int     ki_ngroups;    /* Number of groups */
    dev_t   ki_tdev;       /* TTY device */
    int     ki_flag;       /* Process flags */
    int     ki_stat;       /* Process state */
    char    ki_comm[COMMLEN + 1]; /* Command name */
    char    ki_wmesg[WMESGLEN + 1]; /* Wait channel */
    int     ki_nice;       /* Nice value */
    int     ki_pri;        /* Priority */
    long    ki_size;       /* Virtual size */
    long    ki_rssize;     /* Resident size */
    struct timeval ki_start; /* Start time */
    struct timeval ki_rusage; /* Resource usage */
    /* ... additional fields ... */
};
```

#### KINFO Wrapper

```c
typedef struct {
    struct kinfo_proc *ki_p;
    char *ki_args;      /* Full command line */
    char *ki_env;       /* Environment (if -E) */
    double ki_pcpu;     /* Computed %CPU */
    long ki_memsize;    /* Computed memory size */
} KINFO;
```

#### Format Variable

```c
typedef struct {
    const char *name;    /* Keyword name (e.g., "pid", "user") */
    const char *header;  /* Column header (e.g., "PID", "USER") */
    int width;           /* Column width */
    int (*sprnt)(KINFO *); /* Print function */
    int flag;            /* Format flags */
} VAR;
```

### Constants

```c
#define COMMLEN   256   /* Max command name length */
#define WMESGLEN  64    /* Max wait message length */
#define KI_NGROUPS 16   /* Max supplementary groups tracked */
```

### musl Compatibility

FreeBSD uses `STAILQ_*` macros extensively, but musl's `<sys/queue.h>`
may not provide them. `ps.h` defines custom implementations:

```c
#ifndef STAILQ_HEAD
#define STAILQ_HEAD(name, type)                         \
struct name {                                           \
    struct type *stqh_first;                            \
    struct type **stqh_last;                            \
}
#define STAILQ_ENTRY(type)                              \
struct {                                                \
    struct type *stqe_next;                             \
}
#define STAILQ_INIT(head) do { ... } while (0)
#define STAILQ_INSERT_TAIL(head, elm, field) do { ... } while (0)
#define STAILQ_FOREACH(var, head, field) ...
#endif
```

### Predefined Format Strings

```c
/* Default format (-f not specified) */
const char *dfmt = "pid,tt,stat,time,command";

/* Jobs format (-j) */
const char *jfmt = "user,pid,ppid,pgid,sid,jobc,stat,tt,time,command";

/* Long format (-l) */
const char *lfmt = "uid,pid,ppid,cpu,pri,nice,vsz,rss,wchan,stat,tt,time,command";

/* User format (-u) */
const char *ufmt = "user,pid,%cpu,%mem,vsz,rss,tt,stat,start,time,command";

/* Virtual memory format (-v) */
const char *vfmt = "pid,stat,time,sl,re,pagein,vsz,rss,lim,tsiz,%cpu,%mem,command";
```

### /proc Parsing

Process data is read from multiple `/proc/[pid]/` files:

| File | Data Extracted |
|------|----------------|
| `/proc/[pid]/stat` | PID, PPID, PGID, state, priority, nice, threads, start time |
| `/proc/[pid]/status` | UID, GID, groups, memory (VmSize, VmRSS) |
| `/proc/[pid]/cmdline` | Full command line arguments |
| `/proc/[pid]/environ` | Environment variables (if requested) |
| `/proc/[pid]/wchan` | Wait channel name |
| `/proc/[pid]/fd/0` | Controlling TTY detection |

### Process Filtering

```c
/* Option string */
#define PS_ARGS "AaCcD:defG:gHhjJ:LlM:mN:O:o:p:rSTt:U:uvwXxZ"

struct listinfo {
    int count;
    int maxcount;
    int *list;        /* Array of values to match */
    int (*addelem)(struct listinfo *, const char *);
};
```

Filtering by PID, UID, GID, TTY, session, and process group uses
`struct listinfo` with dynamic arrays and element-specific parsers.

### Column Formatting (keyword.c)

The keyword table maps format names to print functions:

```c
static VAR var[] = {
    {"pid",     "PID",     5, s_pid,     0},
    {"ppid",    "PPID",    5, s_ppid,    0},
    {"user",    "USER",    8, s_user,    0},
    {"uid",     "UID",     5, s_uid,     0},
    {"gid",     "GID",     5, s_gid,     0},
    {"%cpu",    "%CPU",    4, s_pcpu,    0},
    {"%mem",    "%MEM",    4, s_pmem,    0},
    {"vsz",     "VSZ",     6, s_vsz,     0},
    {"rss",     "RSS",     5, s_rss,     0},
    {"tt",      "TT",      3, s_tty,     0},
    {"stat",    "STAT",    4, s_stat,    0},
    {"time",    "TIME",    8, s_time,    0},
    {"command", "COMMAND", 16, s_command, COMM},
    {"args",    "COMMAND", 16, s_args,    COMM},
    {"comm",    "COMMAND", 16, s_comm,    COMM},
    {"nice",    "NI",      3, s_nice,    0},
    {"pri",     "PRI",     3, s_pri,     0},
    {"wchan",   "WCHAN",   8, s_wchan,   0},
    {"start",   "STARTED", 8, s_start,   0},
    /* ... more keywords ... */
    {NULL, NULL, 0, NULL, 0},  /* Sentinel */
};
```

### Global State

```c
int cflag;          /* Raw CPU usage */
int eval;           /* Exit value */
time_t now;         /* Current time */
int rawcpu;         /* Don't compute decay */
int sumrusage;      /* Sum child usage */
int termwidth;      /* Terminal width */
int showthreads;    /* Show threads (-H) */
int hlines;         /* Header repeat interval */
```

## Options Reference

| Flag | Description |
|------|-------------|
| `-A` / `-e` | All processes |
| `-a` | Processes with terminals (except session leaders) |
| `-C` | Raw CPU percentage |
| `-c` | Show command name only (not full path) |
| `-d` | All except session leaders |
| `-f` | Full format |
| `-G gid` | Filter by real group ID |
| `-g group` | Filter by group name |
| `-H` | Show threads |
| `-h` | Repeat header every screenful |
| `-j` | Jobs format |
| `-L` | Show all threads (LWP) |
| `-l` | Long format |
| `-M` | Display MAC label |
| `-m` | Sort by memory usage |
| `-O fmt` | Add columns to default format |
| `-o fmt` | Custom output format |
| `-p pid` | Filter by PID |
| `-r` | Running processes only |
| `-S` | Include child time |
| `-T` | Show threads for current terminal |
| `-t tty` | Filter by TTY |
| `-U user` | Filter by effective user |
| `-u` | User format |
| `-v` | Virtual memory format |
| `-w` | Wide output |
| `-X` | Skip processes without controlling TTY |
| `-x` | Include processes without controlling TTY |
| `-Z` | Show security context |

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `opendir(3)` / `readdir(3)` | Enumerate `/proc/` PIDs |
| `open(2)` / `read(2)` | Read `/proc/[pid]/*` files |
| `stat(2)` | Get file owner for UID detection |
| `getpwuid(3)` / `getgrgid(3)` | UID/GID to name resolution |
| `ioctl(TIOCGWINSZ)` | Terminal width |
| `sysconf(3)` | Clock ticks, page size |

## Examples

```sh
# Default process list
ps

# All processes, user format
ps aux

# Full format
ps -ef

# Custom columns
ps -o pid,user,%cpu,%mem,command

# Filter by user
ps -U root

# Jobs format
ps -j

# Long format with threads
ps -lH
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Error |

## Linux-Specific Notes

- Reads from `/proc` filesystem instead of BSD `kvm_getprocs(3)`
- Custom `struct kinfo_proc` replaces BSD's `<sys/user.h>` variant
- STAILQ macros defined inline for musl compatibility
- No jail (`-J`) support on Linux
- No Capsicum sandboxing
