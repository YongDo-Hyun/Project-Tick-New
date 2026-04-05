# post-receive Hook — Deep Analysis

## Table of Contents

- [Introduction](#introduction)
- [File Location and Deployment](#file-location-and-deployment)
- [Complete Source Listing](#complete-source-listing)
- [Line-by-Line Analysis](#line-by-line-analysis)
  - [Line 1: Shebang](#line-1-shebang)
  - [Lines 2–33: Documentation Header](#lines-2-33-documentation-header)
  - [Line 35: Strict Mode](#line-35-strict-mode)
  - [Lines 41–42: Variable Initialization](#lines-41-42-variable-initialization)
  - [Lines 45–47: Remote Auto-Detection](#lines-45-47-remote-auto-detection)
  - [Lines 49–52: Empty Remote Guard](#lines-49-52-empty-remote-guard)
  - [Lines 57–61: The log() Function](#lines-57-61-the-log-function)
  - [Line 67: Trigger Banner](#line-67-trigger-banner)
  - [Lines 70–74: Stdin Ref Reading Loop](#lines-70-74-stdin-ref-reading-loop)
  - [Lines 76–77: Result Arrays](#lines-76-77-result-arrays)
  - [Lines 79–90: Mirror Push Loop](#lines-79-90-mirror-push-loop)
  - [Lines 92–94: Summary Logging](#lines-92-94-summary-logging)
  - [Lines 97–109: Failure Notification](#lines-97-109-failure-notification)
  - [Lines 112–116: Exit Logic](#lines-112-116-exit-logic)
- [Data Flow Analysis](#data-flow-analysis)
  - [Input Data](#input-data)
  - [Internal State](#internal-state)
  - [Output Channels](#output-channels)
- [Bash Constructs Reference](#bash-constructs-reference)
- [Error Handling Strategy](#error-handling-strategy)
- [Pipeline Behavior Under pipefail](#pipeline-behavior-under-pipefail)
- [Race Conditions and Concurrency](#race-conditions-and-concurrency)
- [Performance Characteristics](#performance-characteristics)
- [Testing the Hook](#testing-the-hook)
  - [Manual Invocation](#manual-invocation)
  - [Dry Run Approach](#dry-run-approach)
  - [Unit Testing with Mocks](#unit-testing-with-mocks)
- [Modification Guide](#modification-guide)
  - [Adding a New Remote Type](#adding-a-new-remote-type)
  - [Adding Retry Logic](#adding-retry-logic)
  - [Adding Webhook Notifications](#adding-webhook-notifications)
  - [Selective Ref Mirroring](#selective-ref-mirroring)
- [Comparison with Alternative Approaches](#comparison-with-alternative-approaches)

---

## Introduction

The `post-receive` hook at `hooks/post-receive` is the single operational hook in the Project-Tick hooks system. It implements multi-forge mirror synchronization — whenever a push lands on the canonical bare repository, this script replicates all refs to every configured mirror remote.

This document provides an exhaustive, line-by-line analysis of the script, covering every variable, control structure, and design decision.

---

## File Location and Deployment

**Source location** (in the monorepo):
```
Project-Tick/hooks/post-receive
```

**Deployed location** (in the bare repository):
```
/path/to/project-tick.git/hooks/post-receive
```

**File type**: Bash shell script  
**Permissions required**: Executable (`chmod +x`)  
**Interpreter**: `/usr/bin/env bash` (portable shebang)  
**Total lines**: 116  

---

## Complete Source Listing

For reference, the complete script with line numbers:

```bash
  1  #!/usr/bin/env bash
  2  # ==============================================================================
  3  # post-receive hook — Mirror push to multiple forges
  4  # ==============================================================================
  5  #
  6  # Place this file in your bare repository:
  7  #   /path/to/project-tick.git/hooks/post-receive
  8  #
  9  # Make it executable:
 10  #   chmod +x hooks/post-receive
 11  #
 12  # Configuration:
 13  #   Set mirror remotes in the bare repo:
 14  #
 15  #   git remote add github  git@github.com:Project-Tick/Project-Tick.git
 16  #   git remote add gitlab  git@gitlab.com:Project-Tick/Project-Tick.git
 17  #   git remote add codeberg git@codeberg.org:Project-Tick/Project-Tick.git
 18  #   git remote add sourceforge ssh://USERNAME@git.code.sf.net/p/project-tick/code
 19  #
 20  #   Or use HTTPS with token auth:
 21  #
 22  #   git remote add github  https://x-access-token:TOKEN@github.com/Project-Tick/Project-Tick.git
 23  #   git remote add gitlab  https://oauth2:TOKEN@gitlab.com/Project-Tick/Project-Tick.git
 24  #   git remote add codeberg https://TOKEN@codeberg.org/Project-Tick/Project-Tick.git
 25  #
 26  #   Environment variables (optional):
 27  #     MIRROR_REMOTES — space-separated list of remote names to push to.
 28  #                      Defaults to all configured mirror remotes.
 29  #     MIRROR_LOG     — path to log file. Defaults to /var/log/git-mirror.log
 30  #     MIRROR_NOTIFY  — email address for failure notifications (requires mail cmd)
 31  #
 32  # ==============================================================================
 33
 34  set -euo pipefail
 35
 36  # ---------------------
 37  # Configuration
 38  # ---------------------
 39
 40  MIRROR_REMOTES="${MIRROR_REMOTES:-}"
 41  MIRROR_LOG="${MIRROR_LOG:-/var/log/git-mirror.log}"
 42
 43  if [[ -z "$MIRROR_REMOTES" ]]; then
 44      MIRROR_REMOTES=$(git remote | grep -v '^origin$' || true)
 45  fi
 46
 47  if [[ -z "$MIRROR_REMOTES" ]]; then
 48      echo "[mirror] No mirror remotes configured. Skipping." >&2
 49      exit 0
 50  fi
 51
 52  # ---------------------
 53  # Logging
 54  # ---------------------
 55  log() {
 56      local timestamp
 57      timestamp="$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
 58      echo "[$timestamp] $*" | tee -a "$MIRROR_LOG" 2>/dev/null || echo "[$timestamp] $*"
 59  }
 60
 61  # ---------------------
 62  # Main
 63  # ---------------------
 64
 65  log "=== Mirror push triggered ==="
 66
 67  REFS=()
 68  while read -r oldrev newrev refname; do
 69      REFS+=("$refname")
 70      log "  ref: $refname ($oldrev -> $newrev)"
 71  done
 72
 73  FAILED_REMOTES=()
 74  SUCCEEDED_REMOTES=()
 75
 76  for remote in $MIRROR_REMOTES; do
 77      log "Pushing to remote: $remote"
 78
 79      if git push --mirror --force "$remote" 2>&1 | tee -a "$MIRROR_LOG" 2>/dev/null; then
 80          SUCCEEDED_REMOTES+=("$remote")
 81          log "  ✓ Successfully pushed to $remote"
 82      else
 83          FAILED_REMOTES+=("$remote")
 84          log "  ✗ FAILED to push to $remote"
 85      fi
 86  done
 87
 88  log "--- Summary ---"
 89  log "  Succeeded: ${SUCCEEDED_REMOTES[*]:-none}"
 90  log "  Failed:    ${FAILED_REMOTES[*]:-none}"
 91
 92  if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_NOTIFY:-}" ]]; then
 93      if command -v mail &>/dev/null; then
 94          {
 95              echo "Mirror push failed for the following remotes:"
 96              printf '  - %s\n' "${FAILED_REMOTES[@]}"
 97              echo ""
 98              echo "Repository: $(pwd)"
 99              echo "Refs updated:"
100              printf '  %s\n' "${REFS[@]}"
101              echo ""
102              echo "Check log: $MIRROR_LOG"
103          } | mail -s "[git-mirror] Push failure in $(basename "$(pwd)")" "$MIRROR_NOTIFY"
104      fi
105  fi
106
107  if [[ ${#FAILED_REMOTES[@]} -gt 0 ]]; then
108      log "=== Finished with errors ==="
109      exit 1
110  fi
111
112  log "=== Finished successfully ==="
113  exit 0
```

---

## Line-by-Line Analysis

### Line 1: Shebang

```bash
#!/usr/bin/env bash
```

The `#!/usr/bin/env bash` shebang is the portable way to invoke bash. Instead of hardcoding `/bin/bash` (which varies across systems — on NixOS, for example, bash is at `/run/current-system/sw/bin/bash`), `env` searches `$PATH` for the `bash` binary.

**Why bash specifically?** The script uses bash-specific features:
- Arrays (`REFS=()`, `REFS+=()`)
- `[[ ]]` conditional expressions
- `${array[*]:-default}` expansion
- `${#array[@]}` array length
- `set -o pipefail`

These are not available in POSIX `sh`.

### Lines 2–33: Documentation Header

The header block is an extensive comment documenting:

1. **What the hook does** — "Mirror push to multiple forges"
2. **Where to deploy it** — `/path/to/project-tick.git/hooks/post-receive`
3. **How to make it executable** — `chmod +x hooks/post-receive`
4. **How to configure mirror remotes** — four SSH examples plus three HTTPS examples
5. **Environment variables** — `MIRROR_REMOTES`, `MIRROR_LOG`, `MIRROR_NOTIFY`

This self-documenting style means an administrator can understand the hook without reading external documentation.

### Line 35: Strict Mode

```bash
set -euo pipefail
```

This is bash "strict mode," composed of three flags:

**`-e` (errexit)**: If any command returns a non-zero exit code, the script terminates immediately. Exceptions:
- Commands in `if` conditions
- Commands followed by `&&` or `||`
- Commands in `while`/`until` conditions

This is why the `git push` is wrapped in `if` — to capture its exit code without triggering `errexit`.

**`-u` (nounset)**: Referencing an unset variable causes an immediate error instead of silently expanding to an empty string. This catches typos like `$MIIROR_LOG`. The `${VAR:-default}` syntax is used throughout to safely reference variables that may not be set.

**`-o pipefail`**: By default, a pipeline's exit code is the exit code of the last command. With `pipefail`, the pipeline's exit code is the exit code of the rightmost command that failed (non-zero). This matters for:

```bash
git push --mirror --force "$remote" 2>&1 | tee -a "$MIRROR_LOG"
```

Without `pipefail`, this pipeline would succeed as long as `tee` succeeds, even if `git push` fails. With `pipefail`, a `git push` failure propagates through the pipeline. However, note the `2>/dev/null` after `tee` which may affect this — see the [Pipeline Behavior Under pipefail](#pipeline-behavior-under-pipefail) section.

### Lines 41–42: Variable Initialization

```bash
MIRROR_REMOTES="${MIRROR_REMOTES:-}"
MIRROR_LOG="${MIRROR_LOG:-/var/log/git-mirror.log}"
```

The `${VAR:-default}` expansion works as follows:

| `VAR` state | Expansion |
|-------------|-----------|
| Set to value | The value |
| Set to empty string | The default |
| Unset | The default |

For `MIRROR_REMOTES`, the default is an empty string, which triggers auto-detection later. For `MIRROR_LOG`, the default is `/var/log/git-mirror.log`.

Note that `MIRROR_NOTIFY` is **not** initialized here — it's referenced later with `${MIRROR_NOTIFY:-}` inline. This is safe because the `:-` syntax prevents `set -u` from triggering on an unset variable.

### Lines 45–47: Remote Auto-Detection

```bash
if [[ -z "$MIRROR_REMOTES" ]]; then
    MIRROR_REMOTES=$(git remote | grep -v '^origin$' || true)
fi
```

**`git remote`** — Lists all remote names, one per line. In the bare repository, this might output:

```
origin
github
gitlab
codeberg
sourceforge
```

**`grep -v '^origin$'`** — Inverts the match, removing lines that are exactly `origin`. The `^` and `$` anchors prevent matching remotes like `origin-backup` or `my-origin`.

**`|| true`** — If `grep` finds no matches (all remotes are `origin`, or there are no remotes at all), it exits with code 1. Under `set -e`, this would terminate the script. The `|| true` ensures the command always succeeds.

**`$(...)`** — Command substitution captures the output. Multi-line output from `git remote` is collapsed into a space-separated string when assigned to a scalar variable, which is exactly what the `for remote in $MIRROR_REMOTES` loop expects.

### Lines 49–52: Empty Remote Guard

```bash
if [[ -z "$MIRROR_REMOTES" ]]; then
    echo "[mirror] No mirror remotes configured. Skipping." >&2
    exit 0
fi
```

If auto-detection produced no results (no non-origin remotes), the script prints a message to stderr (`>&2`) and exits with code 0. Using stderr ensures the message doesn't interfere with any stdout processing, while exit code 0 ensures the push appears successful to the user.

### Lines 57–61: The log() Function

```bash
log() {
    local timestamp
    timestamp="$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    echo "[$timestamp] $*" | tee -a "$MIRROR_LOG" 2>/dev/null || echo "[$timestamp] $*"
}
```

Detailed breakdown:

1. **`local timestamp`** — Declares `timestamp` as function-local. Without `local`, it would be a global variable that persists after the function returns.

2. **`date -u '+%Y-%m-%d %H:%M:%S UTC'`** — Generates a UTC timestamp. The `-u` flag is critical for server environments where multiple time zones may be in play. The format string produces output like `2026-04-05 14:30:00 UTC`.

3. **`echo "[$timestamp] $*"`** — `$*` expands all function arguments as a single string. Unlike `$@`, which preserves argument boundaries, `$*` joins them with the first character of `$IFS` (default: space). For logging, this distinction doesn't matter.

4. **`| tee -a "$MIRROR_LOG"`** — `tee -a` appends (`-a`) to the log file while passing through to stdout. This achieves dual output — the message appears in the hook's stdout (visible to the pusher) and is persisted in the log file.

5. **`2>/dev/null`** — Suppresses `tee`'s stderr. If `$MIRROR_LOG` doesn't exist or isn't writable, `tee` would print an error like `tee: /var/log/git-mirror.log: Permission denied`. Suppressing this keeps the output clean.

6. **`|| echo "[$timestamp] $*"`** — If the entire `echo | tee` pipeline fails (e.g., the log file is unwritable and `tee` exits non-zero under `pipefail`), this fallback ensures the message still reaches stdout.

### Line 67: Trigger Banner

```bash
log "=== Mirror push triggered ==="
```

A visual separator in the log that marks the start of a new mirror operation. The `===` delimiters make it easy to grep for session boundaries:

```bash
grep "=== Mirror push" /var/log/git-mirror.log
```

### Lines 70–74: Stdin Ref Reading Loop

```bash
REFS=()
while read -r oldrev newrev refname; do
    REFS+=("$refname")
    log "  ref: $refname ($oldrev -> $newrev)"
done
```

**`REFS=()`** — Initializes an empty bash array to accumulate ref names.

**`read -r oldrev newrev refname`** — Reads one line from stdin, splitting on whitespace into three variables. The `-r` flag prevents backslash interpretation (e.g., `\n` is read literally, not as a newline).

Git feeds post-receive hooks with lines formatted as:
```
<40-char old SHA-1> <40-char new SHA-1> <refname>
```

The `refname` variable captures everything after the second space, which is correct because ref names don't contain spaces.

**Special SHA values**:

| Old SHA | New SHA | Meaning |
|---------|---------|---------|
| `0000...0000` | `abc123...` | New ref created (branch/tag created) |
| `abc123...` | `def456...` | Ref updated (normal push) |
| `abc123...` | `0000...0000` | Ref deleted (branch/tag deleted) |

**`REFS+=("$refname")`** — Appends the ref name to the array. The quotes around `$refname` are important to preserve the value as a single array element.

### Lines 76–77: Result Arrays

```bash
FAILED_REMOTES=()
SUCCEEDED_REMOTES=()
```

Two arrays that accumulate results as the push loop iterates. These are used later for the summary log and the notification email.

### Lines 79–90: Mirror Push Loop

```bash
for remote in $MIRROR_REMOTES; do
    log "Pushing to remote: $remote"

    if git push --mirror --force "$remote" 2>&1 | tee -a "$MIRROR_LOG" 2>/dev/null; then
        SUCCEEDED_REMOTES+=("$remote")
        log "  ✓ Successfully pushed to $remote"
    else
        FAILED_REMOTES+=("$remote")
        log "  ✗ FAILED to push to $remote"
    fi
done
```

**`for remote in $MIRROR_REMOTES`** — Note the unquoted `$MIRROR_REMOTES`. This is intentional — word splitting on spaces produces individual remote names. If it were quoted as `"$MIRROR_REMOTES"`, the entire string would be treated as a single remote name.

**`git push --mirror --force "$remote"`**:
- `--mirror` — Push all refs under `refs/` to the remote, and delete remote refs that don't exist locally. This includes `refs/heads/*`, `refs/tags/*`, `refs/notes/*`, `refs/replace/*`, etc.
- `--force` — Force-update diverged refs. Without this, pushes to refs that have been rewritten (e.g., after a force-push to the canonical repo) would be rejected.
- `"$remote"` — Quoted to handle remote names with unusual characters (defensive coding).

**`2>&1`** — Merges stderr into stdout. Git's push progress and error messages go to stderr by default; this redirect ensures they're all captured by `tee`.

**`| tee -a "$MIRROR_LOG" 2>/dev/null`** — Appends the complete push output to the log file. The `2>/dev/null` suppresses errors from `tee` if the log isn't writable.

**`if ... then ... else`** — The `if` statement tests the exit code of the pipeline. Under `pipefail`, the pipeline fails if `git push` fails (regardless of `tee`'s exit code).

### Lines 92–94: Summary Logging

```bash
log "--- Summary ---"
log "  Succeeded: ${SUCCEEDED_REMOTES[*]:-none}"
log "  Failed:    ${FAILED_REMOTES[*]:-none}"
```

**`${SUCCEEDED_REMOTES[*]:-none}`** — Expands the array elements separated by spaces. If the array is empty, the `:-none` default kicks in and prints "none". This produces output like:

```
[2026-04-05 14:30:05 UTC] --- Summary ---
[2026-04-05 14:30:05 UTC]   Succeeded: github gitlab codeberg
[2026-04-05 14:30:05 UTC]   Failed:    none
```

### Lines 97–109: Failure Notification

```bash
if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_NOTIFY:-}" ]]; then
    if command -v mail &>/dev/null; then
        {
            echo "Mirror push failed for the following remotes:"
            printf '  - %s\n' "${FAILED_REMOTES[@]}"
            echo ""
            echo "Repository: $(pwd)"
            echo "Refs updated:"
            printf '  %s\n' "${REFS[@]}"
            echo ""
            echo "Check log: $MIRROR_LOG"
        } | mail -s "[git-mirror] Push failure in $(basename "$(pwd)")" "$MIRROR_NOTIFY"
    fi
fi
```

**`${#FAILED_REMOTES[@]}`** — Array length operator. Returns the number of elements in `FAILED_REMOTES`.

**`-gt 0`** — "Greater than 0" — at least one remote failed.

**`-n "${MIRROR_NOTIFY:-}"`** — Tests if `MIRROR_NOTIFY` is non-empty. The `:-` prevents `set -u` from triggering on an unset variable.

**`command -v mail &>/dev/null`** — Checks if `mail` is available. `command -v` is the POSIX-compliant way to check for command existence (preferred over `which`).

**`{ ... } | mail ...`** — A command group constructs the email body as a multi-line string, piping it to `mail`:
- `printf '  - %s\n' "${FAILED_REMOTES[@]}"` — Prints each failed remote as a bulleted list item
- `$(pwd)` — The bare repository path
- `printf '  %s\n' "${REFS[@]}"` — Lists all refs that were updated
- `$MIRROR_LOG` — Points to the log file for detailed output

**`mail -s "..." "$MIRROR_NOTIFY"`** — Sends an email with the given subject line to the configured address.

### Lines 112–116: Exit Logic

```bash
if [[ ${#FAILED_REMOTES[@]} -gt 0 ]]; then
    log "=== Finished with errors ==="
    exit 1
fi

log "=== Finished successfully ==="
exit 0
```

The exit code is meaningful but not catastrophic:

- **`exit 1`** — Git displays the hook's output to the pusher with a warning that the hook failed. The push itself has already succeeded (refs were already updated before `post-receive` ran).
- **`exit 0`** — Clean completion, no warning displayed.

---

## Data Flow Analysis

### Input Data

```
┌──────────────────────────────────────────────────────────┐
│                        stdin                             │
│  <old-sha> <new-sha> refs/heads/main                    │
│  <old-sha> <new-sha> refs/tags/v1.0.0                   │
│  ...                                                     │
└──────────────────────────────────────────────────────────┘
         │
         ▼
  while read -r oldrev newrev refname
         │
         ├──► REFS[] array (refname values)
         └──► log output (old→new transitions)
```

### Internal State

```
┌─────────────────────────────────────────┐
│  MIRROR_REMOTES     "github gitlab ..." │
│  MIRROR_LOG         "/var/log/..."      │
│  MIRROR_NOTIFY      "admin@..." or ""   │
│  REFS[]             ref names from push │
│  FAILED_REMOTES[]   failed remote names │
│  SUCCEEDED_REMOTES[] ok remote names    │
└─────────────────────────────────────────┘
```

### Output Channels

| Channel | Target | Content |
|---------|--------|---------|
| stdout | Pusher's terminal | Log messages, push output |
| `$MIRROR_LOG` | Log file on disk | All log messages + push output |
| `mail` | Email recipient | Failure notification body |
| Exit code | Git server | 0 (success) or 1 (failure) |

---

## Bash Constructs Reference

| Construct | Line(s) | Meaning |
|-----------|---------|---------|
| `${VAR:-default}` | 40–41 | Use `default` if `VAR` is unset or empty |
| `${VAR:-}` | 92 | Expand to empty string if unset (avoids `set -u` error) |
| `$(command)` | 44, 57, 98, 103 | Command substitution |
| `[[ -z "$VAR" ]]` | 43, 47 | Test if string is empty |
| `[[ -n "$VAR" ]]` | 92 | Test if string is non-empty |
| `${#ARRAY[@]}` | 92, 107 | Array length |
| `${ARRAY[*]:-x}` | 89, 90 | All elements or default |
| `ARRAY+=("item")` | 69, 80, 83 | Append to array |
| `read -r a b c` | 68 | Read space-delimited fields |
| `cmd 2>&1` | 79 | Redirect stderr to stdout |
| `cmd &>/dev/null` | 93 | Redirect all output to null |
| `\|\| true` | 44 | Force success exit code |
| `local var` | 56 | Function-scoped variable |
| `{ ... }` | 94–102 | Command group for I/O redirection |

---

## Error Handling Strategy

The script uses a layered error handling approach:

1. **Global strict mode** (`set -euo pipefail`) catches programming errors
2. **`if` wrappers** protect commands that are expected to fail (git push)
3. **`|| true` guards** prevent `set -e` from triggering on grep no-match
4. **`2>/dev/null` + `||` fallback** in `log()` handles unwritable log files
5. **`command -v` checks** prevent crashes when optional tools are missing
6. **`${VAR:-}` expansions** prevent `set -u` errors on optional variables

This means the script will:
- ✓ Continue if one mirror push fails (handled by `if`)
- ✓ Continue if the log file is unwritable (handled by `2>/dev/null || echo`)
- ✓ Continue if `mail` is not installed (handled by `command -v` check)
- ✓ Continue if no remotes are configured (handled by `exit 0` guard)
- ✗ Abort on undefined variables (caught by `set -u`)
- ✗ Abort on unexpected command failures (caught by `set -e`)

---

## Pipeline Behavior Under pipefail

The push pipeline deserves special attention:

```bash
git push --mirror --force "$remote" 2>&1 | tee -a "$MIRROR_LOG" 2>/dev/null
```

Under `pipefail`, the pipeline's exit code is determined by the rightmost failing command:

| `git push` exit | `tee` exit | Pipeline exit |
|-----------------|------------|---------------|
| 0 (success) | 0 (success) | 0 |
| 128 (failure) | 0 (success) | 128 |
| 0 (success) | 1 (failure) | 1 |
| 128 (failure) | 1 (failure) | 128 |

If both fail, the rightmost failure wins — but in practice, `tee` rarely fails because its stderr is redirected to `/dev/null`, and even if it can't write to the log file, it still passes data through to stdout (which always works).

However, there's a subtlety: `tee`'s `2>/dev/null` only suppresses `tee`'s own error messages. If `tee` can't open the log file for writing, it will still exit with a non-zero code, which could mask a `git push` success under `pipefail`. In practice, this is unlikely to cause problems because `tee` typically succeeds even if it can't write (it still outputs to stdout).

---

## Race Conditions and Concurrency

If multiple pushes arrive simultaneously, multiple instances of `post-receive` may run concurrently. Potential issues:

1. **Log file interleaving** — Multiple `tee -a` writes to the same log file. The `-a` (append) mode is file-system atomic for writes smaller than `PIPE_BUF` (typically 4096 bytes), so individual log lines won't corrupt each other, but they may interleave.

2. **Simultaneous mirror pushes** — Two hooks pushing to the same mirror remote concurrently. Git handles this gracefully — one push will complete first, and the second will either fast-forward or be a no-op.

3. **REFS array** — Each hook instance has its own `REFS` array (separate bash process), so there's no cross-instance contamination.

---

## Performance Characteristics

| Operation | Typical Duration | Notes |
|-----------|-----------------|-------|
| Remote auto-detection | <10 ms | `git remote` + `grep` on local config |
| Stdin reading | <1 ms | Reading a few lines from pipe |
| `git push --mirror` per remote | 1–60 seconds | Network-bound; depends on delta size |
| Logging | <1 ms per call | Local file I/O |
| Email notification | 100–500 ms | Depends on MTA |

Total execution time is dominated by the mirror push loop. With 4 remotes, worst case is ~4 minutes for large pushes. The pushes are **sequential**, not parallel — see [Modification Guide](#modification-guide) for adding parallelism.

---

## Testing the Hook

### Manual Invocation

Simulate a push by feeding ref data on stdin:

```bash
cd /path/to/project-tick.git
echo "0000000000000000000000000000000000000000 $(git rev-parse HEAD) refs/heads/main" \
  | hooks/post-receive
```

### Dry Run Approach

Create a modified version that uses `echo` instead of `git push`:

```bash
# In the hook, temporarily replace:
#   git push --mirror --force "$remote"
# With:
#   echo "[DRY RUN] Would push --mirror --force to $remote"
```

### Unit Testing with Mocks

```bash
#!/usr/bin/env bash
# test-post-receive.sh — Test the hook with mock remotes

# Create a temporary bare repo
TMPDIR=$(mktemp -d)
git init --bare "$TMPDIR/test.git"
cd "$TMPDIR/test.git"

# Add a mock remote (pointing to a local bare repo)
git init --bare "$TMPDIR/mirror.git"
git remote add testmirror "$TMPDIR/mirror.git"

# Copy the hook
cp /path/to/hooks/post-receive hooks/post-receive
chmod +x hooks/post-receive

# Create a dummy ref
git hash-object -t commit --stdin <<< "tree $(git hash-object -t tree /dev/null)
author Test <test@test> 0 +0000
committer Test <test@test> 0 +0000

test" > /dev/null

# Invoke the hook
echo "0000000000000000000000000000000000000000 $(git rev-parse HEAD 2>/dev/null || echo abc123) refs/heads/main" \
  | MIRROR_LOG="$TMPDIR/mirror.log" hooks/post-receive

echo "Exit code: $?"
cat "$TMPDIR/mirror.log"

# Cleanup
rm -rf "$TMPDIR"
```

---

## Modification Guide

### Adding a New Remote Type

Simply add a new git remote to the bare repository. No script modification needed:

```bash
cd /path/to/project-tick.git
git remote add bitbucket git@bitbucket.org:Project-Tick/Project-Tick.git
```

The auto-detection mechanism will pick it up automatically on the next push.

### Adding Retry Logic

To add retry logic for transient network failures, replace the push section:

```bash
for remote in $MIRROR_REMOTES; do
    log "Pushing to remote: $remote"
    
    MAX_RETRIES=3
    RETRY_DELAY=5
    attempt=0
    push_success=false
    
    while [[ $attempt -lt $MAX_RETRIES ]]; do
        attempt=$((attempt + 1))
        log "  Attempt $attempt/$MAX_RETRIES for $remote"
        
        if git push --mirror --force "$remote" 2>&1 | tee -a "$MIRROR_LOG" 2>/dev/null; then
            push_success=true
            break
        fi
        
        if [[ $attempt -lt $MAX_RETRIES ]]; then
            log "  Retrying in ${RETRY_DELAY}s..."
            sleep "$RETRY_DELAY"
        fi
    done
    
    if $push_success; then
        SUCCEEDED_REMOTES+=("$remote")
        log "  ✓ Successfully pushed to $remote"
    else
        FAILED_REMOTES+=("$remote")
        log "  ✗ FAILED to push to $remote after $MAX_RETRIES attempts"
    fi
done
```

### Adding Webhook Notifications

To add webhook notifications (e.g., Slack, Discord) alongside email:

```bash
# After the mail block, add:
if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_WEBHOOK:-}" ]]; then
    if command -v curl &>/dev/null; then
        PAYLOAD=$(cat <<EOF
{
    "text": "Mirror push failed in $(basename "$(pwd)")",
    "remotes": "$(printf '%s, ' "${FAILED_REMOTES[@]}")",
    "refs": "$(printf '%s, ' "${REFS[@]}")"
}
EOF
)
        curl -s -X POST -H "Content-Type: application/json" \
            -d "$PAYLOAD" "$MIRROR_WEBHOOK" 2>/dev/null || true
    fi
fi
```

### Selective Ref Mirroring

To mirror only specific branches instead of using `--mirror`:

```bash
for remote in $MIRROR_REMOTES; do
    for ref in "${REFS[@]}"; do
        log "Pushing $ref to $remote"
        if git push --force "$remote" "$ref" 2>&1 | tee -a "$MIRROR_LOG" 2>/dev/null; then
            log "  ✓ $ref -> $remote"
        else
            log "  ✗ FAILED $ref -> $remote"
            FAILED_REMOTES+=("$remote:$ref")
        fi
    done
done
```

---

## Comparison with Alternative Approaches

| Approach | Pros | Cons |
|----------|------|------|
| **post-receive hook** (current) | Simple, self-contained, zero external deps | Sequential pushes, coupled to git server |
| **CI-triggered mirror** | Parallel, retries built-in, monitoring | Requires CI infrastructure, higher latency |
| **Cron-based sync** | Decoupled from push flow | Delayed mirroring, may miss rapid pushes |
| **Git federation** | Native, protocol-level | Not widely supported |
| **Grokmirror** | Efficient for large repos | Complex setup, Python dependency |

The post-receive hook approach chosen by Project-Tick is the simplest and most appropriate for a single-repository setup where immediate mirroring is desired.
