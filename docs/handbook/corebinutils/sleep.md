# sleep — Suspend Execution for an Interval

## Overview

`sleep` pauses for the specified duration. It supports fractional seconds,
multiple arguments (accumulated), unit suffixes (`s`, `m`, `h`, `d`),
and `SIGINFO`/`SIGUSR1` for progress reporting.

**Source**: `sleep/sleep.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
sleep number[suffix] ...
```

## Options

No flags. Arguments are durations with optional unit suffixes.

## Unit Suffixes

| Suffix | Meaning | Multiplier |
|--------|---------|------------|
| `s` (default) | Seconds | 1 |
| `m` | Minutes | 60 |
| `h` | Hours | 3600 |
| `d` | Days | 86400 |

## Source Analysis

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse arguments and sleep loop |
| `parse_interval()` | Parse numeric value with unit suffix |
| `scale_interval()` | Apply unit multiplier with overflow check |
| `seconds_to_timespec()` | Convert float seconds to `struct timespec` |
| `seconds_from_timespec()` | Extract seconds from `struct timespec` |
| `install_info_handler()` | Set up `SIGINFO`/`SIGUSR1` handler |
| `report_remaining()` | Print remaining time on signal |
| `die()` / `die_errno()` | Error handling |
| `usage()` | Print usage and exit |

### Argument Accumulation

Multiple arguments are summed:

```c
int main(int argc, char *argv[])
{
    double total = 0.0;

    for (int i = 1; i < argc; i++) {
        double interval = parse_interval(argv[i]);
        total += interval;
    }

    if (total > (double)TIME_T_MAX)
        die("total sleep duration too large");

    struct timespec ts = seconds_to_timespec(total);
    install_info_handler();

    /* Sleep loop with EINTR restart */
    while (nanosleep(&ts, &ts) < 0) {
        if (errno != EINTR)
            die_errno("nanosleep");

        /* SIGINFO handler may have reported progress */
    }

    return 0;
}
```

### Interval Parsing

```c
static double
parse_interval(const char *arg)
{
    char *end;
    double val = strtod(arg, &end);

    if (end == arg || val < 0)
        die("invalid time interval: %s", arg);

    /* Apply unit suffix */
    if (*end != '\0') {
        val = scale_interval(val, *end);
        end++;
    }

    if (*end != '\0')
        die("invalid time interval: %s", arg);

    return val;
}

static double
scale_interval(double val, char unit)
{
    switch (unit) {
    case 's': return val;
    case 'm': return val * 60.0;
    case 'h': return val * 3600.0;
    case 'd': return val * 86400.0;
    default:
        die("invalid unit: %c", unit);
    }
}
```

### Progress Reporting

```c
static volatile sig_atomic_t info_requested;

static void
signal_handler(int sig)
{
    (void)sig;
    info_requested = 1;
}

static void
install_info_handler(void)
{
    struct sigaction sa = {
        .sa_handler = signal_handler,
        .sa_flags = 0,
    };
    sigemptyset(&sa.sa_mask);

#ifdef SIGINFO
    sigaction(SIGINFO, &sa, NULL);
#endif
    sigaction(SIGUSR1, &sa, NULL);
}

static void
report_remaining(const struct timespec *remaining)
{
    double secs = seconds_from_timespec(remaining);
    fprintf(stderr, "sleep: about %.1f second(s) remaining\n", secs);
    info_requested = 0;
}
```

When `nanosleep` returns with `EINTR` and the remaining time is in `ts`,
the handler flag is checked and progress is reported before restarting.

### Overflow Protection

```c
static struct timespec
seconds_to_timespec(double sec)
{
    struct timespec ts;

    if (sec >= (double)TIME_T_MAX) {
        ts.tv_sec = TIME_T_MAX;
        ts.tv_nsec = 0;
    } else {
        ts.tv_sec = (time_t)sec;
        ts.tv_nsec = (long)((sec - ts.tv_sec) * 1e9);
    }

    return ts;
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `nanosleep(2)` | Sleep with nanosecond precision |
| `sigaction(2)` | Install signal handlers |

## Examples

```sh
# Sleep 5 seconds
sleep 5

# Fractional seconds
sleep 0.5

# With units
sleep 2m      # 2 minutes
sleep 1.5h    # 90 minutes
sleep 1d      # 24 hours

# Multiple arguments (accumulated)
sleep 1m 30s  # 90 seconds total

# Check remaining time (send SIGUSR1 from another terminal)
kill -USR1 $(pgrep sleep)
# → "sleep: about 42.3 second(s) remaining"
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success (slept for full duration) |
| 1    | Error (invalid argument) |

## Differences from GNU sleep

- POSIX-compliant with BSD extensions
- Supports `SIGINFO` (on systems that have it, otherwise `SIGUSR1`)
- Same unit suffix support (`s`, `m`, `h`, `d`)
- Multiple arguments are accumulated (same as GNU)
