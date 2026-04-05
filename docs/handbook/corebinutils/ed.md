# ed — Line Editor

## Overview

`ed` is the standard POSIX line editor. It operates on a text buffer that
resides in a temporary scratch file, supports regular expression search and
substitution, global commands, undo, and file I/O. This implementation derives
from Andrew Moore's BSD `ed` and the algorithm described in Kernighan and
Plauger's *Software Tools in Pascal*.

**Source**: `ed/main.c`, `ed/ed.h`, `ed/compat.c`, `ed/compat.h`, `ed/buf.c`,
`ed/glbl.c`, `ed/io.c`, `ed/re.c`, `ed/sub.c`, `ed/undo.c`
**Origin**: BSD 4.4, Andrew Moore (Talke Studio)
**License**: BSD-2-Clause

## Synopsis

```
ed [-] [-sx] [-p string] [file]
```

## Options

| Flag | Description |
|------|-------------|
| `-` | Suppress diagnostics (same as `-s`) |
| `-s` | Script mode: suppress byte counts and `!` prompts |
| `-x` | Encryption mode (**not supported on Linux**) |
| `-p string` | Set the command prompt (default: no prompt) |

## Source Architecture

### File Responsibilities

| File | Purpose | Key Functions |
|------|---------|---------------|
| `main.c` | Main loop, command dispatch, signals | `main()`, `exec_command()`, signal handlers |
| `ed.h` | Types, constants, function prototypes | `line_t`, `undo_t`, error codes |
| `compat.c/h` | Linux portability shims | `strlcpy`, `strlcat` replacements |
| `buf.c` | Scratch file buffer management | `get_sbuf_line()`, `put_sbuf_line()` |
| `glbl.c` | Global command (g/re/cmd) | `exec_global()`, mark management |
| `io.c` | File I/O (read/write) | `read_file()`, `write_file()`, `read_stream()` |
| `re.c` | Regular expression handling | `get_compiled_pattern()`, `search_*()` |
| `sub.c` | Substitution command | `substitute()`, replacement parsing |
| `undo.c` | Undo stack management | `push_undo_stack()`, `undo_last()` |

### Key Data Structures

#### Line Node (linked list element)

```c
typedef struct line {
    struct line *q_forw;   /* Next line in buffer */
    struct line *q_back;   /* Previous line in buffer */
    off_t        seek;     /* Byte offset in scratch file */
    int          len;      /* Line length */
} line_t;
```

The edit buffer is a doubly-linked circular list of `line_t` nodes. Line
content is stored in an external scratch file, not in memory.

#### Undo Record

```c
typedef struct undo {
    int type;              /* UADD, UDEL, UMOV, VMOV */
    line_t *h;             /* Head of affected line range */
    line_t *t;             /* Tail of affected line range */
    /* ... */
} undo_t;
```

#### Constants

```c
#define ERR   (-2)    /* General error */
#define EMOD  (-3)    /* Buffer modified warning */
#define FATAL (-4)    /* Fatal error (abort) */

#define MINBUFSZ 512
#define SE_MAX   30   /* Max regex subexpressions */
#define LINECHARS INT_MAX
```

#### Global Flags

```c
#define GLB  001   /* Global command active */
#define GPR  002   /* Print after command */
#define GLS  004   /* List after command */
#define GNP  010   /* Enumerate after command */
#define GSG  020   /* Global substitute */
```

### Main Loop

```c
int main(volatile int argc, char **volatile argv)
{
    setlocale(LC_ALL, "");

    /* Detect if invoked as "red" (restricted ed) */
    red = (n = strlen(argv[0])) > 2 && argv[0][n - 3] == 'r';

    /* Parse options */
    while ((c = getopt(argc, argv, "p:sx")) != -1) { ... }

    /* Signal setup */
    signal(SIGHUP, signal_hup);    /* Emergency save */
    signal(SIGQUIT, SIG_IGN);      /* Ignore quit */
    signal(SIGINT, signal_int);    /* Interrupt handling */
    signal(SIGWINCH, handle_winch); /* Terminal resize */

    /* Initialize buffers, load file if specified */
    init_buffers();
    if (argc && is_legal_filename(*argv))
        read_file(*argv, 0);

    /* Command loop */
    for (;;) {
        if (prompt) fputs(prompt, stdout);
        status = get_tty_line();
        if (status == EOF) break;
        status = exec_command();
    }
}
```

### Buffer Management (buf.c)

The scratch file strategy avoids unlimited memory consumption:

- Lines are stored in a temporary file created with `mkstemp(3)`
- `put_sbuf_line()` appends a line to the scratch file and returns its offset
- `get_sbuf_line()` reads a line back from the scratch file by offset
- The `line_t` linked list tracks offsets and lengths, not actual text

```c
/* Append line to scratch file, return its node */
line_t *put_sbuf_line(const char *text);

/* Read line from scratch file via offset */
char *get_sbuf_line(const line_t *lp);
```

### File I/O (io.c)

```c
long read_file(char *fn, long n)
{
    /* Open file or pipe (if fn starts with '!') */
    fp = (*fn == '!') ? popen(fn + 1, "r") : fopen(strip_escapes(fn), "r");

    /* Read lines into buffer after line n */
    size = read_stream(fp, n);

    /* Print byte count unless in script mode */
    if (!scripted)
        fprintf(stdout, "%lu\n", size);
}
```

The `read_stream()` function reads from `fp`, appending each line to the
edit buffer via `put_sbuf_line()`, and maintaining the undo stack for
rollback.

### Regular Expressions (re.c)

Uses POSIX `regex.h` (via `regcomp(3)` / `regexec(3)`):

```c
typedef regex_t pattern_t;

pattern_t *get_compiled_pattern(void);
/* Compiles the current regex pattern, caching the last used pattern */
```

### Substitution (sub.c)

The `s/pattern/replacement/flags` command:
- Supports `\1` through `\9` backreferences
- `g` flag for global replacement
- Count for nth occurrence replacement
- `&` in replacement refers to the matched text

### Undo (undo.c)

Every buffer modification pushes an undo record:

```c
undo_t *push_undo_stack(int type, long from, long to);
int undo_last(void);   /* Reverse last modification */
```

Undo types: `UADD` (lines added), `UDEL` (lines deleted), `UMOV` (lines moved).

### Signal Handling

| Signal | Handler | Action |
|--------|---------|--------|
| `SIGHUP` | `signal_hup()` | Save buffer to `ed.hup` and exit |
| `SIGINT` | `signal_int()` | Set interrupt flag, longjmp to command prompt |
| `SIGWINCH` | `handle_winch()` | Update terminal width for `l` command |
| `SIGQUIT` | `SIG_IGN` | Ignored |

### Restricted Mode (red)

When invoked as `red`, the editor restricts:
- Shell commands (`!command`) are forbidden
- Filenames with `/` or starting with `!` are rejected
- Directory changes are prevented

## Commands Reference

| Command | Description |
|---------|-------------|
| `(.)a` | Append text after line |
| `(.)i` | Insert text before line |
| `(.,.)c` | Change (replace) lines |
| `(.,.)d` | Delete lines |
| `(.,.)p` | Print lines |
| `(.,.)l` | List lines (show non-printable characters) |
| `(.,.)n` | Number and print lines |
| `(.,.)m(.)` | Move lines |
| `(.,.)t(.)` | Copy (transfer) lines |
| `(.,.)s/re/replacement/flags` | Substitute |
| `(.,.)g/re/command` | Global: apply command to matching lines |
| `(.,.)v/re/command` | Inverse global: apply to non-matching lines |
| `(.,.)w file` | Write lines to file |
| `(.,.)W file` | Append lines to file |
| `e file` | Edit file (replaces buffer) |
| `E file` | Edit unconditionally |
| `f file` | Set default filename |
| `r file` | Read file into buffer |
| `(.)r !command` | Read command output into buffer |
| `u` | Undo last command |
| `(.)=` | Print line number |
| `(.,.)j` | Join lines |
| `(.)k(c)` | Mark line with character c |
| `q` | Quit (warns if modified) |
| `Q` | Quit unconditionally |
| `H` | Toggle verbose error messages |
| `h` | Print last error message |
| `!command` | Execute shell command |

### Addressing

| Address | Meaning |
|---------|---------|
| `.` | Current line |
| `$` | Last line |
| `n` | Line number n |
| `-n` / `+n` | Relative to current line |
| `/re/` | Next line matching regex |
| `?re?` | Previous line matching regex |
| `'c` | Line marked with character c |
| `,` | Equivalent to `1,$` |
| `;` | Equivalent to `.,$` |

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `mkstemp(3)` | Create scratch file |
| `read(2)` / `write(2)` | Scratch file I/O |
| `lseek(2)` | Position in scratch file |
| `fopen(3)` / `fclose(3)` | Read/write user files |
| `popen(3)` / `pclose(3)` | Shell command execution |
| `regcomp(3)` / `regexec(3)` | Regular expression matching |
| `sigsetjmp(3)` / `siglongjmp(3)` | Interrupt recovery |

## Examples

```sh
# Edit a file
ed myfile.txt

# Script mode (for automation)
printf '1,3p\nq\n' | ed -s myfile.txt

# Global substitution
printf 'g/old/s//new/g\nw\nq\n' | ed -s myfile.txt

# With prompt
ed -p '> ' myfile.txt

# Read from pipe
echo '!ls -la' | ed
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Error |
| 2    | Usage error |

## Linux-Specific Notes

- The `-x` (encryption) option prints an error and exits, as Linux
  does not provide the BSD `des_setkey(3)` functions.
- `strlcpy(3)` / `strlcat(3)` are provided by `compat.c` when not
  available in the system libc.
- The `SIGWINCH` handler uses `ioctl(TIOCGWINSZ)` for terminal size.
