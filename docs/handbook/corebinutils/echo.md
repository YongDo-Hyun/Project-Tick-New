# echo — Write Arguments to Standard Output

## Overview

`echo` writes its arguments to standard output, separated by spaces, followed
by a newline. It is intentionally minimal — the FreeBSD/BSD implementation
does not support GNU-style `-e` escape processing.

**Source**: `echo/echo.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
echo [-n] [string ...]
```

## Options

| Flag | Description |
|------|-------------|
| `-n` | Suppress trailing newline |

Only a leading `-n` is recognized as an option. Any other arguments
(including `--`) are treated as literal strings and printed.

## Source Analysis

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse arguments and write output |
| `write_all()` | Retry-safe `write(2)` loop handling `EINTR` |
| `warn_errno()` | Error reporting to stderr |
| `trim_trailing_backslash_c()` | Check if final argument ends with `\c` |

### Option Processing

`echo` does NOT use `getopt(3)`. It manually checks for `-n`:

```c
int main(int argc, char *argv[])
{
    bool suppress_newline = false;

    argv++;  /* Skip program name */

    /* Only leading -n flags are consumed */
    while (*argv && strcmp(*argv, "-n") == 0) {
        suppress_newline = true;
        argv++;
    }

    /* Everything else is literal output */
}
```

### The `\c` Convention

If the **last** argument ends with `\c`, the trailing newline is suppressed
and the `\c` itself is not printed:

```c
static bool
trim_trailing_backslash_c(const char *arg, size_t *len)
{
    if (*len >= 2 && arg[*len - 2] == '\\' && arg[*len - 1] == 'c') {
        *len -= 2;
        return true;  /* Suppress newline */
    }
    return false;
}
```

### I/O Strategy

Instead of `printf` or `writev`, echo uses a `write(2)` loop:

```c
static int
write_all(int fd, const void *buf, size_t count)
{
    const char *p = buf;
    ssize_t n;

    while (count > 0) {
        n = write(fd, p, count);
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

This avoids `IOV_MAX` limitations that would apply with `writev(2)` when
there are many arguments.

### Key Behaviors

| Input | Output | Notes |
|-------|--------|-------|
| `echo hello` | `hello\n` | Basic usage |
| `echo -n hello` | `hello` | No trailing newline |
| `echo -n -n hello` | `hello` | Multiple `-n` consumed |
| `echo -- hello` | `-- hello\n` | `--` is NOT end-of-options |
| `echo -e hello` | `-e hello\n` | `-e` is NOT recognized |
| `echo "hello\c"` | `hello` | `\c` suppresses newline |
| `echo ""` | `\n` | Empty string → just newline |

## Portability Notes

- **BSD echo** (this implementation): Only `-n` and trailing `\c`
- **GNU echo**: Supports `-e` for escape sequences (`\n`, `\t`, etc.)
  and `-E` to disable them
- **POSIX echo**: Behavior of `-n` and backslash sequences is
  implementation-defined
- **Shell built-in**: Most shells have a built-in `echo` that may differ
  from the external command

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `write(2)` | All output to stdout |

## Examples

```sh
# Simple output
echo Hello, World!

# No trailing newline
echo -n "prompt> "

# Literal dash-n (only leading -n is recognized)
echo "The flag is -n"

# Multiple arguments
echo one two three
# → "one two three"

# Suppress newline with \c
echo "no newline\c"
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Write error |
