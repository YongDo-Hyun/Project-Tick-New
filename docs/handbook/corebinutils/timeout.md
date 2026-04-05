# timeout — Run a Command with a Time Limit

## Overview

`timeout` runs a command and kills it if it exceeds a time limit. It
supports a two-stage kill strategy: first send a configurable signal
(default `SIGTERM`), then optionally send a second kill signal after
a grace period. Uses `prctl(PR_SET_CHILD_SUBREAPER)` to reliably
reap grandchild processes.

**Source**: `timeout/timeout.c` (single file)
**Origin**: BSD/Project Tick
**License**: BSD-3-Clause

## Synopsis

```
timeout [--preserve-status] [--foreground] [-k duration]
        [-s signal] [--verbose] duration command [arg ...]
```

## Options

| Flag | Description |
|------|-------------|
| `-s signal` | Signal to send on timeout (default: `SIGTERM`) |
| `-k duration` | Kill signal to send after grace period |
| `--preserve-status` | Exit with the command's status, not 124 |
| `--foreground` | Don't create a new process group |
| `--verbose` | Print diagnostics when sending signals |

## Source Analysis

### Constants

```c
#define EXIT_TIMEOUT   124   /* Command timed out */
#define EXIT_INVALID   125   /* timeout itself failed */
#define EXIT_CMD_ERROR 126   /* Command found but not executable */
#define EXIT_CMD_NOENT 127   /* Command not found */
```

### Data Structures

```c
struct options {
    bool foreground;         /* --foreground */
    bool preserve;           /* --preserve-status */
    bool verbose;            /* --verbose */
    bool kill_after_set;     /* -k was specified */
    int timeout_signal;      /* -s signal (default SIGTERM) */
    double duration;         /* Primary timeout */
    double kill_after;       /* Grace period before SIGKILL */
    const char *command_name;
    char **command_argv;
};

struct child_state {
    pid_t pid;
    int status;
    bool exited;
    bool signaled;
};

struct runtime_state {
    struct child_state child;
    bool first_timeout_sent;
    bool kill_sent;
};

enum deadline_kind {
    DEADLINE_TIMEOUT,     /* Primary timeout */
    DEADLINE_KILL,        /* Kill-after grace period */
};
```

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options, fork, wait with timers |
| `parse_duration_or_die()` | Parse duration string (fractional seconds + units) |
| `monotonic_seconds()` | Read `CLOCK_MONOTONIC` |
| `enable_subreaper_or_die()` | Call `prctl(PR_SET_CHILD_SUBREAPER)` |
| `send_signal_to_command()` | Send signal to child/process group |
| `arm_second_timer()` | Set up kill-after timer |
| `reap_children()` | Wait for all descendants |
| `child_exec()` | Child process: exec the command |

### Signal Table

`timeout` shares the same signal table as `kill`:

```c
/* Same SIGNAL_ENTRY() macro and signal_entry table */
/* Supports named signals: TERM, KILL, HUP, INT, etc. */
/* Supports SIGRTMIN+n notation */
```

### Duration Parsing

```c
static double
parse_duration_or_die(const char *str)
{
    char *end;
    double val = strtod(str, &end);

    if (end == str || val < 0)
        errx(EXIT_INVALID, "invalid duration: %s", str);

    /* Apply unit suffix */
    switch (*end) {
    case '\0':
    case 's': break;          /* seconds (default) */
    case 'm': val *= 60; break;
    case 'h': val *= 3600; break;
    case 'd': val *= 86400; break;
    default:
        errx(EXIT_INVALID, "invalid unit: %c", *end);
    }

    return val;
}
```

### Subreaper

The Linux-specific `prctl(PR_SET_CHILD_SUBREAPER)` ensures that orphaned
grandchild processes are reparented to `timeout` instead of PID 1:

```c
static void
enable_subreaper_or_die(void)
{
    if (prctl(PR_SET_CHILD_SUBREAPER, 1) < 0)
        err(EXIT_INVALID, "prctl(PR_SET_CHILD_SUBREAPER)");
}
```

### Two-Stage Kill Strategy

```
┌──────────────────────────────────────────────────┐
│  timeout 30 -k 5 -s TERM ./long_running_task     │
│                                                   │
│  1. Fork and exec ./long_running_task             │
│  2. Wait up to 30 seconds                         │
│  3. If still running: send SIGTERM                 │
│  4. Wait up to 5 more seconds (-k 5)              │
│  5. If still running: send SIGKILL                 │
│  6. Reap all children                              │
└──────────────────────────────────────────────────┘
```

```c
/* Primary timeout handler */
static void
handle_timeout(struct runtime_state *state,
               const struct options *opts)
{
    if (opts->verbose)
        warnx("sending signal %s to command '%s'",
              signal_name_for_number(opts->timeout_signal),
              opts->command_name);

    send_signal_to_command(state, opts->timeout_signal, opts);
    state->first_timeout_sent = true;

    /* Arm kill-after timer if specified */
    if (opts->kill_after_set)
        arm_second_timer(opts->kill_after);
}

/* Kill-after timer handler */
static void
handle_kill_after(struct runtime_state *state,
                  const struct options *opts)
{
    if (opts->verbose)
        warnx("sending SIGKILL to command '%s'",
              opts->command_name);

    send_signal_to_command(state, SIGKILL, opts);
    state->kill_sent = true;
}
```

### Process Group Management

```c
static void
send_signal_to_command(struct runtime_state *state,
                       int sig, const struct options *opts)
{
    if (opts->foreground) {
        /* Send to child only */
        kill(state->child.pid, sig);
    } else {
        /* Send to entire process group */
        kill(-state->child.pid, sig);
    }
}

static void
child_exec(const struct options *opts)
{
    if (!opts->foreground) {
        /* Create new process group */
        setpgid(0, 0);
    }

    execvp(opts->command_name, opts->command_argv);

    /* exec failed */
    int code = (errno == ENOENT) ? EXIT_CMD_NOENT : EXIT_CMD_ERROR;
    err(code, "exec '%s'", opts->command_name);
}
```

### Timer Implementation

Uses `timer_create(2)` with `CLOCK_MONOTONIC`:

```c
static void
arm_timer(double seconds)
{
    struct itimerspec its = {
        .it_value = {
            .tv_sec = (time_t)seconds,
            .tv_nsec = (long)((seconds - (time_t)seconds) * 1e9),
        },
    };

    timer_t timerid;
    struct sigevent sev = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo = SIGALRM,
    };

    timer_create(CLOCK_MONOTONIC, &sev, &timerid);
    timer_settime(timerid, 0, &its, NULL);
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `fork(2)` | Create child process |
| `execvp(3)` | Execute the command |
| `kill(2)` | Send signal to child/group |
| `waitpid(2)` | Wait for child/grandchild exit |
| `setpgid(2)` | Create new process group |
| `prctl(2)` | `PR_SET_CHILD_SUBREAPER` |
| `timer_create(2)` | POSIX timer for deadline |
| `timer_settime(2)` | Arm the timer |
| `clock_gettime(2)` | `CLOCK_MONOTONIC` for elapsed time |
| `sigaction(2)` | Signal handler setup |

## Examples

```sh
# Basic timeout (30 seconds)
timeout 30 make -j4

# With kill-after grace period
timeout -k 10 60 ./server

# Custom signal
timeout -s HUP 300 ./daemon

# Verbose
timeout --verbose 5 sleep 100
# timeout: sending signal TERM to command 'sleep'

# Preserve exit status
timeout --preserve-status 10 ./test_runner
echo $?  # Exit code from test_runner, not 124

# Fractional seconds
timeout 2.5 curl https://example.com

# Foreground (no process group)
timeout --foreground 30 ./interactive_app
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 124  | Command timed out |
| 125  | `timeout` itself failed |
| 126  | Command found but not executable |
| 127  | Command not found |
| other | Command's exit status (or 128+signal if killed) |
