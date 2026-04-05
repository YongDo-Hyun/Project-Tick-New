# kill — Send Signals to Processes

## Overview

`kill` sends signals to processes or lists available signals. This
implementation supports both numeric and named signal specifications,
real-time signals (`SIGRT`), and can be compiled as a shell built-in.

**Source**: `kill/kill.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
kill [-s signal_name] pid ...
kill -l [exit_status ...]
kill -signal_name pid ...
kill -signal_number pid ...
```

## Options

| Flag | Description |
|------|-------------|
| `-s signal` | Send the named signal |
| `-l` | List available signal names |
| `-signal_name` | Send named signal (e.g., `-TERM`) |
| `-signal_number` | Send signal by number (e.g., `-15`) |

## Source Analysis

### Signal Table

The signal table maps names to numbers using a macro-generated array:

```c
struct signal_entry {
    const char *name;
    int number;
};

#define SIGNAL_ENTRY(sig) { #sig, SIG##sig }

static const struct signal_entry signal_table[] = {
    SIGNAL_ENTRY(HUP),
    SIGNAL_ENTRY(INT),
    SIGNAL_ENTRY(QUIT),
    SIGNAL_ENTRY(ILL),
    SIGNAL_ENTRY(TRAP),
    SIGNAL_ENTRY(ABRT),
    SIGNAL_ENTRY(EMT),     /* If available */
    SIGNAL_ENTRY(FPE),
    SIGNAL_ENTRY(KILL),
    SIGNAL_ENTRY(BUS),
    SIGNAL_ENTRY(SEGV),
    SIGNAL_ENTRY(SYS),
    SIGNAL_ENTRY(PIPE),
    SIGNAL_ENTRY(ALRM),
    SIGNAL_ENTRY(TERM),
    SIGNAL_ENTRY(URG),
    SIGNAL_ENTRY(STOP),
    SIGNAL_ENTRY(TSTP),
    SIGNAL_ENTRY(CONT),
    SIGNAL_ENTRY(CHLD),
    SIGNAL_ENTRY(TTIN),
    SIGNAL_ENTRY(TTOU),
    SIGNAL_ENTRY(IO),
    SIGNAL_ENTRY(XCPU),
    SIGNAL_ENTRY(XFSZ),
    SIGNAL_ENTRY(VTALRM),
    SIGNAL_ENTRY(PROF),
    SIGNAL_ENTRY(WINCH),
    SIGNAL_ENTRY(INFO),    /* If available */
    SIGNAL_ENTRY(USR1),
    SIGNAL_ENTRY(USR2),
    /* ... */
};
```

### Key Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options and dispatch signal or list |
| `normalize_signal_name()` | Canonicalize signal name (strip `SIG` prefix, uppercase) |
| `parse_signal_option_token()` | Parse `-SIGNAL` shorthand |
| `parse_signal_for_dash_s()` | Parse signal name/number for `-s` |
| `signal_name_for_number()` | Reverse lookup: number → name |
| `printsignals()` | List all signals (for `-l`) |
| `max_signal_number()` | Find highest valid signal |
| `parse_pid_argument()` | Parse and validate PID string |

### Signal Name Normalization

```c
static const char *
normalize_signal_name(const char *name)
{
    /* Strip optional "SIG" prefix */
    if (strncasecmp(name, "SIG", 3) == 0)
        name += 3;

    /* Case-insensitive lookup in signal_table */
    for (size_t i = 0; i < SIGNAL_TABLE_SIZE; i++) {
        if (strcasecmp(name, signal_table[i].name) == 0)
            return signal_table[i].name;
    }
    return NULL;
}
```

### Parsing Signal Options

The option parsing handles three forms:

```c
/* Form 1: kill -s SIGNAL pid */
/* Form 2: kill -SIGNAL pid (dash prefix) */
/* Form 3: kill -NUMBER pid */

static int
parse_signal_option_token(const char *token)
{
    /* Try as number first */
    char *end;
    long val = strtol(token, &end, 10);
    if (*end == '\0' && val >= 0 && val <= max_signal_number())
        return (int)val;

    /* Try as name */
    const char *name = normalize_signal_name(token);
    if (name) {
        /* Look up number from normalized name */
        return number_for_name(name);
    }

    errx(2, "unknown signal: %s", token);
}
```

### Real-Time Signal Support

```c
/* SIGRTMIN+n and SIGRTMAX-n notation */
#ifdef SIGRTMIN
    if (strncasecmp(name, "RTMIN", 5) == 0) {
        int offset = (name[5] == '+') ? atoi(name + 6) : 0;
        return SIGRTMIN + offset;
    }
    if (strncasecmp(name, "RTMAX", 5) == 0) {
        int offset = (name[5] == '-') ? atoi(name + 6) : 0;
        return SIGRTMAX - offset;
    }
#endif
```

### Listing Signals

```c
static void
printsignals(FILE *fp)
{
    int columns = 0;
    for (int sig = 1; sig <= max_signal_number(); sig++) {
        const char *name = signal_name_for_number(sig);
        if (name) {
            fprintf(fp, "%s", name);
            if (++columns >= 8) {
                fputc('\n', fp);
                columns = 0;
            } else {
                fputc('\t', fp);
            }
        }
    }
}
```

### Signal from Exit Status

When given an exit status with `-l`, the signal number is extracted:

```c
/* exit_status > 128 means killed by signal (exit_status - 128) */
if (exit_status > 128)
    sig = exit_status - 128;
```

### Shell Built-in Integration

```c
#ifdef SHELL
/* When compiled into the shell (sh/), kill is a built-in */
/* Uses different error reporting and argument parsing */
int killcmd(int argc, char *argv[]);
#endif
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `kill(2)` | Send signal to process or process group |

## Examples

```sh
# Send SIGTERM (default)
kill 1234

# Send SIGKILL
kill -9 1234
kill -KILL 1234
kill -s KILL 1234

# Send to process group
kill -TERM -1234

# List all signals
kill -l

# Signal name from exit status
kill -l 137
# → KILL (137 - 128 = 9 = SIGKILL)

# Real-time signal
kill -s RTMIN+3 1234
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | All signals sent successfully |
| 1    | Error sending signal to at least one process |
| 2    | Usage error |
