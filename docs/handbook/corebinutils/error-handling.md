# Error Handling — Corebinutils Patterns

## Overview

Corebinutils uses a layered error handling strategy: BSD `err(3)` functions
as the primary interface, custom `error_errno()`/`error_msg()` wrappers in
utilities that need more control, and consistent exit codes following
POSIX conventions.

## err(3) Family

The BSD `<err.h>` functions are used throughout:

```c
#include <err.h>

/* Fatal errors (print message + errno + exit) */
err(1, "open '%s'", filename);
/* → "utility: open 'file.txt': No such file or directory\n" */

/* Fatal errors (print message + exit, no errno) */
errx(2, "invalid option: -%c", ch);
/* → "utility: invalid option: -z\n" */

/* Non-fatal warnings (print message + errno, continue) */
warn("stat '%s'", filename);
/* → "utility: stat 'file.txt': Permission denied\n" */

/* Non-fatal warnings (print message, no errno, continue) */
warnx("skipping '%s': not a regular file", filename);
/* → "utility: skipping 'foo': not a regular file\n" */
```

### When to Use Each

| Function | Fatal? | Shows errno? | Use Case |
|----------|--------|-------------|----------|
| `err()` | Yes | Yes | Syscall failure, must exit |
| `errx()` | Yes | No | Bad input, usage error |
| `warn()` | No | Yes | Syscall failure, can continue |
| `warnx()` | No | No | Validation issue, can continue |

## Custom Error Functions

Several utilities define their own error reporting for program name
control or additional formatting:

### Pattern: error_errno / error_msg

```c
static const char *progname;

static void
error_errno(const char *fmt, ...)
{
    int saved_errno = errno;
    va_list ap;

    fprintf(stderr, "%s: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", strerror(saved_errno));
}

static void
error_msg(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
```

Used by: `mkdir`, `chmod`, `hostname`, `domainname`, `nproc`

### Pattern: die / die_errno

```c
static void __dead2
die(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void __dead2
die_errno(const char *fmt, ...)
{
    int saved = errno;
    va_list ap;
    fprintf(stderr, "%s: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", strerror(saved));
    exit(1);
}
```

Used by: `sleep`, `echo`

### Pattern: verror_message (centralized)

```c
static void
verror_message(const char *fmt, va_list ap, bool with_errno)
{
    int saved = errno;
    fprintf(stderr, "%s: ", progname);
    vfprintf(stderr, fmt, ap);
    if (with_errno)
        fprintf(stderr, ": %s", strerror(saved));
    fputc('\n', stderr);
}
```

## Exit Code Conventions

### Standard Codes

| Code | Meaning | Used By |
|------|---------|---------|
| 0 | Success | All utilities |
| 1 | General error | Most utilities |
| 2 | Usage/syntax error | test, expr, timeout, mv |

### Utility-Specific Codes

| Utility | Code | Meaning |
|---------|------|---------|
| `test` | 0 | Expression is true |
| `test` | 1 | Expression is false |
| `test` | 2 | Invalid expression |
| `expr` | 0 | Non-null, non-zero result |
| `expr` | 1 | Null or zero result |
| `expr` | 2 | Invalid expression |
| `expr` | 3 | Internal error |
| `timeout` | 124 | Command timed out |
| `timeout` | 125 | `timeout` itself failed |
| `timeout` | 126 | Command not executable |
| `timeout` | 127 | Command not found |

### Exit on First Error vs. Accumulate

Two patterns are observed:

```c
/* Pattern 1: Exit immediately on error */
if (stat(path, &sb) < 0)
    err(1, "stat");

/* Pattern 2: Accumulate errors, exit with status */
int errors = 0;
for (i = 0; i < argc; i++) {
    if (process(argv[i]) < 0) {
        warn("failed: %s", argv[i]);
        errors = 1;
    }
}
return errors;
```

Pattern 2 is used by multi-argument utilities (rm, chmod, cp, ln)
to process as many arguments as possible even when some fail.

## errno Preservation

All error functions save `errno` before calling any function that
might modify it (like `fprintf`):

```c
static void
error_errno(const char *fmt, ...)
{
    int saved = errno;   /* Save before fprintf */
    /* ... */
    fprintf(stderr, ": %s\n", strerror(saved));
}
```

## Signal Error Recovery

### sigsetjmp/siglongjmp (ed)

```c
static sigjmp_buf jmpbuf;

static void
signal_handler(int sig)
{
    (void)sig;
    siglongjmp(jmpbuf, 1);
}

/* In main loop */
if (sigsetjmp(jmpbuf, 1) != 0) {
    /* Returned from signal — reset state */
    fputs("?\n", stderr);
}
```

### Flag-Based (sleep, dd)

```c
static volatile sig_atomic_t got_signal;

static void
handler(int sig)
{
    got_signal = sig;
}

/* In main loop */
if (got_signal) {
    cleanup();
    exit(128 + got_signal);
}
```

## Validation Patterns

### At System Boundaries

```c
/* Validate user input */
if (argc < 2) {
    usage();
    /* NOTREACHED */
}

/* Validate parsed values */
if (val < 0 || val > MAX_VALUE)
    errx(2, "value out of range: %ld", val);

/* Validate system call results */
if (open(path, O_RDONLY) < 0)
    err(1, "open");
```

### String-to-Number Conversion

```c
static long
parse_number(const char *str)
{
    char *end;
    errno = 0;
    long val = strtol(str, &end, 10);

    if (end == str || *end != '\0')
        errx(2, "not a number: %s", str);
    if (errno == ERANGE)
        errx(2, "number out of range: %s", str);

    return val;
}
```

## Write Error Detection

### Pattern: Check stdout at exit

```c
/* Catch write errors (e.g., broken pipe) */
if (fclose(stdout) == EOF)
    err(1, "stdout");

/* Or equivalently */
if (fflush(stdout) == EOF)
    err(1, "write error");
```

### Pattern: write_all loop

```c
static int
write_all(int fd, const void *buf, size_t count)
{
    const char *p = buf;
    while (count > 0) {
        ssize_t n = write(fd, p, count);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += n;
        count -= n;
    }
    return 0;
}
```

Used by: `echo`, `cat`, `dd`

## Summary of Conventions

1. Use `err(3)` family when available and sufficient
2. Define custom wrappers only when program name control is needed
3. Save `errno` immediately — before any library calls
4. Exit 0 for success, 1 for errors, 2 for usage
5. Multi-argument commands accumulate errors
6. Validate at system boundaries (input parsing, syscall returns)
7. Signal handlers set flags only — no complex logic
8. Always check `write(2)` / `fclose(3)` return values
