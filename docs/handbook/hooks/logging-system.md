# Logging System

## Table of Contents

- [Introduction](#introduction)
- [The log() Function](#the-log-function)
  - [Function Signature](#function-signature)
  - [Timestamp Generation](#timestamp-generation)
  - [Dual Output with tee](#dual-output-with-tee)
  - [Graceful Degradation](#graceful-degradation)
- [Log File Configuration](#log-file-configuration)
  - [The MIRROR_LOG Variable](#the-mirror_log-variable)
  - [Default Path](#default-path)
  - [Custom Log Paths](#custom-log-paths)
  - [Log File Permissions](#log-file-permissions)
- [Log Message Format](#log-message-format)
  - [Timestamp Format](#timestamp-format)
  - [Session Boundaries](#session-boundaries)
  - [Ref Update Entries](#ref-update-entries)
  - [Push Status Entries](#push-status-entries)
  - [Summary Block](#summary-block)
- [Complete Log Output Example](#complete-log-output-example)
- [Git Push Output Capture](#git-push-output-capture)
- [Log Rotation](#log-rotation)
  - [Using logrotate](#using-logrotate)
  - [Manual Rotation](#manual-rotation)
  - [Size-Based Rotation](#size-based-rotation)
- [Log Analysis](#log-analysis)
  - [Counting Mirror Sessions](#counting-mirror-sessions)
  - [Finding Failures](#finding-failures)
  - [Extracting Push Duration](#extracting-push-duration)
  - [Monitoring with tail](#monitoring-with-tail)
- [Fallback Behavior](#fallback-behavior)
- [Concurrency and Log Interleaving](#concurrency-and-log-interleaving)
- [Security Considerations](#security-considerations)

---

## Introduction

The Project-Tick `post-receive` hook (`hooks/post-receive`) includes a built-in logging system that records every mirror push operation. The system is implemented as a single bash function, `log()`, that writes timestamped messages to both standard output and a persistent log file.

The logging system is designed with three priorities:
1. **Reliability** — Logging never causes the hook to fail, even if the log file is unwritable
2. **Visibility** — Messages appear on the pusher's terminal and in the persistent log
3. **Simplicity** — A single function, no external logging frameworks

---

## The log() Function

### Function Signature

```bash
log() {
    local timestamp
    timestamp="$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    echo "[$timestamp] $*" | tee -a "$MIRROR_LOG" 2>/dev/null || echo "[$timestamp] $*"
}
```

The function accepts any number of string arguments via `$*`, which concatenates them with a space separator.

**Usage examples** from the hook:

```bash
log "=== Mirror push triggered ==="
log "  ref: $refname ($oldrev -> $newrev)"
log "Pushing to remote: $remote"
log "  ✓ Successfully pushed to $remote"
log "  ✗ FAILED to push to $remote"
log "--- Summary ---"
log "  Succeeded: ${SUCCEEDED_REMOTES[*]:-none}"
log "  Failed:    ${FAILED_REMOTES[*]:-none}"
log "=== Finished with errors ==="
log "=== Finished successfully ==="
```

### Timestamp Generation

```bash
local timestamp
timestamp="$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
```

The `date` command is invoked with two key options:

| Option | Purpose |
|--------|---------|
| `-u` | Use UTC time regardless of the server's local timezone |
| `'+%Y-%m-%d %H:%M:%S UTC'` | ISO 8601-inspired format with explicit UTC suffix |

**Why UTC?** Server environments may span multiple time zones. UTC ensures all log entries are comparable without timezone conversion. The explicit `UTC` suffix in the format string makes it unambiguous — a reader seeing `[2026-04-05 14:30:00 UTC]` knows this is not local time.

**Why `local`?** The `local` keyword restricts `timestamp` to the function scope. Without it, `timestamp` would be a global variable, persisting after the function returns and potentially conflicting with other variables.

### Dual Output with tee

```bash
echo "[$timestamp] $*" | tee -a "$MIRROR_LOG" 2>/dev/null
```

The `tee` command reads from stdin and writes to both stdout and the specified file:

```
echo "message" ──► tee ──► stdout (pusher's terminal)
                    │
                    └──► $MIRROR_LOG (append mode)
```

The `-a` flag means **append**. Without it, `tee` would truncate the log file on each write, losing previous entries.

### Graceful Degradation

```bash
echo "[$timestamp] $*" | tee -a "$MIRROR_LOG" 2>/dev/null || echo "[$timestamp] $*"
```

The error handling chain works in three stages:

1. **Primary path**: `echo | tee -a "$MIRROR_LOG"` — write to both stdout and log file
2. **Error suppression**: `2>/dev/null` — if `tee` can't write to the log file, suppress its error message (e.g., "Permission denied")
3. **Fallback**: `|| echo "[$timestamp] $*"` — if the entire `echo | tee` pipeline fails, write to stdout only

This means the log function **never fails silently** — even if the log file is inaccessible, the message still reaches the pusher's terminal. And it **never crashes the hook** — log file errors don't propagate despite `set -e` (because they're handled by the `||` fallback).

---

## Log File Configuration

### The MIRROR_LOG Variable

```bash
MIRROR_LOG="${MIRROR_LOG:-/var/log/git-mirror.log}"
```

The log file path is controlled by the `MIRROR_LOG` environment variable with a default fallback.

### Default Path

The default log file location is `/var/log/git-mirror.log`. This follows the Linux Filesystem Hierarchy Standard (FHS) convention of placing log files under `/var/log/`.

**Requirements for the default path**:
- The directory `/var/log/` must exist (it always does on standard Linux systems)
- The user running the git daemon must have write permission to the file
- The file will be created if it doesn't exist (assuming directory write permission)

### Custom Log Paths

Override the default by setting `MIRROR_LOG` in the hook's environment:

```bash
# In the git daemon's environment
export MIRROR_LOG=/var/log/project-tick/mirror.log

# Or per-repository via a wrapper
MIRROR_LOG=/home/git/logs/mirror.log hooks/post-receive
```

**Common custom paths**:

| Path | Use Case |
|------|----------|
| `/var/log/git-mirror.log` | Default — shared system log |
| `/var/log/project-tick/mirror.log` | Project-specific log directory |
| `/home/git/logs/mirror.log` | User-local log (no root needed) |
| `/tmp/mirror.log` | Temporary/testing |
| `/dev/null` | Disable file logging (stdout only) |

### Log File Permissions

Set up the log file with appropriate ownership:

```bash
# Create the log file with correct ownership
sudo touch /var/log/git-mirror.log
sudo chown git:git /var/log/git-mirror.log
sudo chmod 640 /var/log/git-mirror.log

# Or create a project-specific log directory
sudo mkdir -p /var/log/project-tick
sudo chown git:git /var/log/project-tick
sudo chmod 750 /var/log/project-tick
```

The `640` permission (`rw-r-----`) allows the git user to write and the git group to read, while preventing other users from accessing potentially sensitive information.

---

## Log Message Format

### Timestamp Format

Every log line follows this pattern:

```
[YYYY-MM-DD HH:MM:SS UTC] <message>
```

Example:
```
[2026-04-05 14:30:00 UTC] === Mirror push triggered ===
```

The square brackets delimit the timestamp, making it easy to parse programmatically:

```bash
# Extract just the messages (remove timestamps)
sed 's/^\[[^]]*\] //' /var/log/git-mirror.log

# Extract just the timestamps
grep -oP '^\[\K[^]]+' /var/log/git-mirror.log
```

### Session Boundaries

Each mirror operation is delimited by banner lines:

```
[2026-04-05 14:30:00 UTC] === Mirror push triggered ===
...
[2026-04-05 14:30:15 UTC] === Finished successfully ===
```

Or on failure:

```
[2026-04-05 14:30:00 UTC] === Mirror push triggered ===
...
[2026-04-05 14:30:15 UTC] === Finished with errors ===
```

The `===` delimiters serve as visual and programmatic session markers.

### Ref Update Entries

Each ref in the push is logged with indentation:

```
[2026-04-05 14:30:00 UTC]   ref: refs/heads/main (abc1234 -> def5678)
[2026-04-05 14:30:00 UTC]   ref: refs/tags/v1.0.0 (0000000 -> abc1234)
```

The format `($oldrev -> $newrev)` shows the transition. The all-zeros SHA (`0000000...`) indicates a new ref creation or deletion:

| Pattern | Meaning |
|---------|---------|
| `(000... -> abc...)` | New ref created |
| `(abc... -> def...)` | Ref updated |
| `(abc... -> 000...)` | Ref deleted |

### Push Status Entries

Each remote push produces a status line:

```
[2026-04-05 14:30:05 UTC] Pushing to remote: github
[2026-04-05 14:30:08 UTC]   ✓ Successfully pushed to github
```

Or on failure:

```
[2026-04-05 14:30:10 UTC] Pushing to remote: sourceforge
[2026-04-05 14:30:25 UTC]   ✗ FAILED to push to sourceforge
```

The Unicode symbols (✓ and ✗) provide quick visual scanning in the log.

### Summary Block

At the end of each session:

```
[2026-04-05 14:30:15 UTC] --- Summary ---
[2026-04-05 14:30:15 UTC]   Succeeded: github gitlab codeberg
[2026-04-05 14:30:15 UTC]   Failed:    sourceforge
```

Or when all succeed:

```
[2026-04-05 14:30:15 UTC] --- Summary ---
[2026-04-05 14:30:15 UTC]   Succeeded: github gitlab codeberg sourceforge
[2026-04-05 14:30:15 UTC]   Failed:    none
```

---

## Complete Log Output Example

A typical successful mirror operation produces:

```
[2026-04-05 14:30:00 UTC] === Mirror push triggered ===
[2026-04-05 14:30:00 UTC]   ref: refs/heads/main (a1b2c3d4e5f6 -> f6e5d4c3b2a1)
[2026-04-05 14:30:00 UTC]   ref: refs/tags/v2.1.0 (0000000000000000000000000000000000000000 -> a1b2c3d4e5f6)
[2026-04-05 14:30:00 UTC] Pushing to remote: github
To github.com:Project-Tick/Project-Tick.git
 + a1b2c3d..f6e5d4c main -> main (forced update)
 * [new tag]         v2.1.0 -> v2.1.0
[2026-04-05 14:30:03 UTC]   ✓ Successfully pushed to github
[2026-04-05 14:30:03 UTC] Pushing to remote: gitlab
To gitlab.com:Project-Tick/Project-Tick.git
 + a1b2c3d..f6e5d4c main -> main (forced update)
 * [new tag]         v2.1.0 -> v2.1.0
[2026-04-05 14:30:06 UTC]   ✓ Successfully pushed to gitlab
[2026-04-05 14:30:06 UTC] Pushing to remote: codeberg
To codeberg.org:Project-Tick/Project-Tick.git
 + a1b2c3d..f6e5d4c main -> main (forced update)
 * [new tag]         v2.1.0 -> v2.1.0
[2026-04-05 14:30:09 UTC]   ✓ Successfully pushed to codeberg
[2026-04-05 14:30:09 UTC] --- Summary ---
[2026-04-05 14:30:09 UTC]   Succeeded: github gitlab codeberg
[2026-04-05 14:30:09 UTC]   Failed:    none
[2026-04-05 14:30:09 UTC] === Finished successfully ===
```

Note that the raw `git push` output (the `To ...` and `+ ... (forced update)` lines) is **interleaved** with the hook's log messages. This is because `git push` output goes through `tee` to the log file alongside the hook's `log()` calls.

---

## Git Push Output Capture

Beyond the `log()` function's messages, the raw output of each `git push` is also captured:

```bash
git push --mirror --force "$remote" 2>&1 | tee -a "$MIRROR_LOG" 2>/dev/null
```

The `2>&1` redirect merges git's stderr into stdout before piping to `tee`. Git sends progress messages and transfer statistics to stderr, so this redirect ensures the complete output is logged:

```
To github.com:Project-Tick/Project-Tick.git
 + a1b2c3d..f6e5d4c main -> main (forced update)
 * [new tag]         v2.1.0 -> v2.1.0
```

This raw output appears in the log file **without timestamps** because it bypasses the `log()` function. It sits between the "Pushing to remote:" and "✓ Successfully pushed" entries.

---

## Log Rotation

The hook appends to the log file indefinitely. Without rotation, the file will grow without bound. Here are strategies for managing log file size.

### Using logrotate

Create `/etc/logrotate.d/git-mirror`:

```
/var/log/git-mirror.log {
    weekly
    rotate 12
    compress
    delaycompress
    missingok
    notifempty
    create 640 git git
}
```

| Directive | Effect |
|-----------|--------|
| `weekly` | Rotate once per week |
| `rotate 12` | Keep 12 rotated files (3 months) |
| `compress` | Compress rotated files with gzip |
| `delaycompress` | Don't compress the most recent rotated file |
| `missingok` | Don't error if the log file doesn't exist |
| `notifempty` | Don't rotate if the file is empty |
| `create 640 git git` | Create new log file with these permissions |

### Manual Rotation

```bash
# Rotate manually
mv /var/log/git-mirror.log /var/log/git-mirror.log.1
touch /var/log/git-mirror.log
chown git:git /var/log/git-mirror.log
```

No signal or restart is needed — the hook appends to `$MIRROR_LOG` on each invocation, so it will create a new file if the old one was moved.

### Size-Based Rotation

Add a cron job that rotates when the file exceeds a certain size:

```bash
# /etc/cron.daily/git-mirror-log-rotate
#!/bin/sh
LOG=/var/log/git-mirror.log
MAX_SIZE=10485760  # 10 MB

if [ -f "$LOG" ]; then
    SIZE=$(stat -c %s "$LOG" 2>/dev/null || echo 0)
    if [ "$SIZE" -gt "$MAX_SIZE" ]; then
        mv "$LOG" "${LOG}.$(date +%Y%m%d)"
        gzip "${LOG}.$(date +%Y%m%d)"
    fi
fi
```

---

## Log Analysis

### Counting Mirror Sessions

```bash
grep -c "=== Mirror push triggered ===" /var/log/git-mirror.log
```

### Finding Failures

```bash
# Find all failure entries
grep "✗ FAILED" /var/log/git-mirror.log

# Find sessions that ended with errors
grep "=== Finished with errors ===" /var/log/git-mirror.log

# Count failures per remote
grep "✗ FAILED" /var/log/git-mirror.log | awk '{print $NF}' | sort | uniq -c | sort -rn
```

### Extracting Push Duration

Calculate the time between trigger and finish:

```bash
# Extract session start and end times
grep -E "(Mirror push triggered|Finished)" /var/log/git-mirror.log
```

### Monitoring with tail

For real-time monitoring during a push:

```bash
tail -f /var/log/git-mirror.log
```

---

## Fallback Behavior

The logging system handles the following failure scenarios:

| Scenario | Behavior |
|----------|----------|
| Log file doesn't exist | `tee` creates it (if directory is writable) |
| Log file is not writable | `tee` error suppressed; message goes to stdout only |
| Log directory doesn't exist | `tee` fails silently; message goes to stdout only |
| `/dev/null` as log path | All file output discarded; stdout works normally |
| `$MIRROR_LOG` is empty | `tee -a ""` fails; fallback echo to stdout |

In every case, the hook continues to function. Logging is strictly best-effort and never causes a hook failure.

---

## Concurrency and Log Interleaving

When multiple pushes trigger the hook simultaneously, multiple `post-receive` instances write to the same log file concurrently. The `-a` (append) flag on `tee` uses `O_APPEND` semantics, which means writes are atomic at the kernel level for sizes up to `PIPE_BUF` (4096 bytes on Linux).

Since individual log lines are well under 4096 bytes, **individual lines will not be corrupted**. However, lines from different sessions may interleave:

```
[2026-04-05 14:30:00 UTC] === Mirror push triggered ===       # Session A
[2026-04-05 14:30:00 UTC] === Mirror push triggered ===       # Session B
[2026-04-05 14:30:00 UTC]   ref: refs/heads/main (...)        # Session A
[2026-04-05 14:30:00 UTC]   ref: refs/heads/feature (...)     # Session B
```

To disambiguate, you could modify the `log()` function to include a PID:

```bash
echo "[$$][$timestamp] $*" | tee -a "$MIRROR_LOG" 2>/dev/null || echo "[$$][$timestamp] $*"
```

This produces lines like `[12345][2026-04-05 14:30:00 UTC] message` which can be filtered by PID.

---

## Security Considerations

1. **Log file contents** — The log records ref names, remote names, and git push output. It should **not** contain credentials (tokens are in the git config, not in push output). However, treat the log as moderately sensitive.

2. **Log file permissions** — Use `640` or `600` permissions. Avoid world-readable (`644`) logs on multi-user systems.

3. **Log injection** — Ref names come from the pusher and appear in log messages (`log "  ref: $refname ..."`). While this is a cosmetic concern (log files aren't executed), extremely long or crafted ref names could produce misleading log entries. Git itself limits ref names to valid characters.

4. **Disk exhaustion** — Without log rotation, the log file grows indefinitely. A hostile actor with push access could trigger many pushes to fill the disk. Use log rotation and monitoring to mitigate.
