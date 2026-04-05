# Hooks — Overview

## Table of Contents

- [Introduction](#introduction)
- [What Are Git Hooks?](#what-are-git-hooks)
- [Hook Types in Git](#hook-types-in-git)
  - [Client-Side Hooks](#client-side-hooks)
  - [Server-Side Hooks](#server-side-hooks)
- [Project-Tick Hook Architecture](#project-tick-hook-architecture)
- [The post-receive Hook](#the-post-receive-hook)
  - [Purpose and Design Goals](#purpose-and-design-goals)
  - [Script Anatomy](#script-anatomy)
  - [Configuration Block](#configuration-block)
  - [Remote Auto-Detection](#remote-auto-detection)
  - [Logging Subsystem](#logging-subsystem)
  - [Main Execution Loop](#main-execution-loop)
  - [Push Strategy](#push-strategy)
  - [Result Tracking](#result-tracking)
  - [Failure Notification](#failure-notification)
  - [Exit Behavior](#exit-behavior)
- [Supported Forge Targets](#supported-forge-targets)
  - [GitHub](#github)
  - [GitLab](#gitlab)
  - [Codeberg](#codeberg)
  - [SourceForge](#sourceforge)
- [Authentication Methods](#authentication-methods)
  - [SSH Key Authentication](#ssh-key-authentication)
  - [HTTPS Token Authentication](#https-token-authentication)
- [Environment Variables](#environment-variables)
- [Installation Guide](#installation-guide)
- [Directory Layout](#directory-layout)
- [Operational Flow Diagram](#operational-flow-diagram)
- [Interaction with Other Project-Tick Components](#interaction-with-other-project-tick-components)
- [Troubleshooting Common Issues](#troubleshooting-common-issues)
- [Security Considerations](#security-considerations)
- [Related Documentation](#related-documentation)

---

## Introduction

The `hooks/` directory in the Project-Tick monorepo contains Git hook scripts that automate repository management tasks. These hooks are designed to run on the bare repository that serves as the canonical upstream source for the Project-Tick project.

The hooks system currently consists of a single, well-structured script:

| File | Type | Purpose |
|------|------|---------|
| `hooks/post-receive` | Bash script | Mirror pushes to multiple forge platforms |

This document provides a comprehensive explanation of how the hooks system works, how Git hooks function in general, and how the Project-Tick hook integrates with the broader project infrastructure.

---

## What Are Git Hooks?

Git hooks are executable scripts that Git runs automatically at specific points in the version control workflow. They reside in the `.git/hooks/` directory of a repository (or the `hooks/` directory of a bare repository). Git ships with sample hook scripts (with `.sample` extensions) that are inactive by default.

Hooks serve as extension points for automating tasks such as:

- Enforcing commit message conventions
- Running linters or tests before accepting commits
- Triggering CI/CD pipelines after pushes
- Synchronizing mirrors to external platforms
- Sending notifications on repository events

A hook is activated by placing an executable file with the correct name (no extension) in the hooks directory. Git invokes the hook at the corresponding event and passes relevant data via standard input or command-line arguments.

---

## Hook Types in Git

### Client-Side Hooks

Client-side hooks run on the developer's local machine during operations like committing, merging, and rebasing:

| Hook | Trigger | Use Case |
|------|---------|----------|
| `pre-commit` | Before a commit is created | Lint source files, check formatting |
| `prepare-commit-msg` | After default message generated | Auto-populate commit templates |
| `commit-msg` | After user enters message | Validate commit message format |
| `post-commit` | After commit completes | Post-commit notifications |
| `pre-rebase` | Before rebase starts | Prevent rebasing published branches |
| `post-merge` | After a merge completes | Restore tracked file permissions |
| `pre-push` | Before push to remote | Run tests before sharing code |
| `post-checkout` | After `git checkout` | Set up working directory environment |

### Server-Side Hooks

Server-side hooks run on the repository that receives pushes. These are the hooks relevant to the Project-Tick infrastructure:

| Hook | Trigger | Input | Use Case |
|------|---------|-------|----------|
| `pre-receive` | Before any refs updated | `<old-sha> <new-sha> <refname>` per line on stdin | Reject pushes that violate policies |
| `update` | Per-ref, before each ref updated | `<refname> <old-sha> <new-sha>` as arguments | Per-branch access control |
| `post-receive` | After all refs updated | `<old-sha> <new-sha> <refname>` per line on stdin | Trigger CI, mirrors, notifications |
| `post-update` | After refs updated | Updated ref names as arguments | Update `info/refs` for dumb HTTP |

The **post-receive** hook is the one used by Project-Tick. It fires after all refs have been successfully updated, making it the ideal place for mirror synchronization — the push to the canonical repo has already succeeded, so mirroring can proceed without blocking the original pusher.

---

## Project-Tick Hook Architecture

The Project-Tick hooks system follows a minimal, single-script architecture:

```
hooks/
└── post-receive          # The only hook script — handles multi-forge mirroring
```

The script is stored in the monorepo source tree at `hooks/post-receive` and is deployed to the bare repository at the path:

```
/path/to/project-tick.git/hooks/post-receive
```

### Design Principles

1. **Single responsibility** — The script does exactly one thing: mirror pushes to configured forge remotes.
2. **Fail-safe defaults** — If no mirror remotes are configured, the script exits silently without error.
3. **Comprehensive logging** — Every action is logged with UTC timestamps.
4. **Non-blocking on partial failure** — If one remote fails, the script continues pushing to the remaining remotes.
5. **Notification support** — Optional email alerts on failure.
6. **Zero external dependencies** — Uses only bash builtins, `git`, `date`, `tee`, and optionally `mail`.

---

## The post-receive Hook

### Purpose and Design Goals

The `post-receive` script in `hooks/post-receive` serves as a multi-forge mirror synchronization tool. When a push lands on the canonical bare repository, this hook automatically replicates all refs (branches, tags, notes) to every configured mirror remote.

The opening comment block documents this purpose:

```bash
# ==============================================================================
# post-receive hook — Mirror push to multiple forges
# ==============================================================================
```

### Script Anatomy

The script is structured into four clearly delineated sections:

1. **Header block** (lines 1–33) — Shebang, documentation comments, and usage instructions
2. **Configuration block** (lines 35–53) — Variable initialization and remote auto-detection
3. **Logging function** (lines 55–62) — The `log()` helper
4. **Main execution** (lines 64–112) — Ref reading, push loop, summary, notification, exit

The script begins with strict error handling:

```bash
#!/usr/bin/env bash
set -euo pipefail
```

The `set -euo pipefail` line enables three safety nets:

| Flag | Effect |
|------|--------|
| `-e` | Exit immediately if any command fails |
| `-u` | Treat unset variables as errors |
| `-o pipefail` | A pipeline fails if any component fails, not just the last command |

### Configuration Block

The configuration block initializes three environment-controlled variables:

```bash
MIRROR_REMOTES="${MIRROR_REMOTES:-}"
MIRROR_LOG="${MIRROR_LOG:-/var/log/git-mirror.log}"
```

| Variable | Default | Purpose |
|----------|---------|---------|
| `MIRROR_REMOTES` | `""` (empty — triggers auto-detection) | Space-separated list of git remote names |
| `MIRROR_LOG` | `/var/log/git-mirror.log` | Path to the log file |
| `MIRROR_NOTIFY` | `""` (unset — notifications disabled) | Email address for failure alerts |

The `${VAR:-default}` syntax provides defaults while allowing environment variable overrides. This means an administrator can control behavior without modifying the script:

```bash
MIRROR_REMOTES="github gitlab" MIRROR_LOG=/tmp/mirror.log ./hooks/post-receive
```

### Remote Auto-Detection

If `MIRROR_REMOTES` is empty (the default), the script auto-detects mirror targets:

```bash
if [[ -z "$MIRROR_REMOTES" ]]; then
    MIRROR_REMOTES=$(git remote | grep -v '^origin$' || true)
fi
```

This runs `git remote` to list all configured remotes, then filters out `origin` with `grep -v '^origin$'`. The `|| true` suffix prevents `set -e` from terminating the script if `grep` finds no matches (which would produce exit code 1).

The rationale: `origin` typically points to the canonical repository itself. Everything else is assumed to be a mirror target. This convention allows adding new mirrors simply by running:

```bash
git remote add <name> <url>
```

If after auto-detection the list is still empty, the script exits cleanly:

```bash
if [[ -z "$MIRROR_REMOTES" ]]; then
    echo "[mirror] No mirror remotes configured. Skipping." >&2
    exit 0
fi
```

This is a **non-error exit** (`exit 0`) because having no mirrors is a valid configuration — the hook should not cause the push to appear to have failed.

### Logging Subsystem

The `log()` function provides timestamped logging to both stdout and a persistent log file:

```bash
log() {
    local timestamp
    timestamp="$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    echo "[$timestamp] $*" | tee -a "$MIRROR_LOG" 2>/dev/null || echo "[$timestamp] $*"
}
```

Key characteristics:

- **UTC timestamps** — `date -u` ensures consistent timestamps regardless of server timezone.
- **Format** — `[2026-04-05 14:30:00 UTC] message` — ISO 8601 date with human-readable time.
- **Dual output** — `tee -a` appends to `$MIRROR_LOG` while also writing to stdout.
- **Graceful fallback** — If the log file is not writable (permissions, missing directory), `2>/dev/null` suppresses the `tee` error, and the `||` fallback ensures the message still appears on stdout.
- **`local` variable** — The `timestamp` variable is scoped to the function to avoid polluting the global namespace.

### Main Execution Loop

The main section begins by reading the ref update data from stdin:

```bash
log "=== Mirror push triggered ==="

REFS=()
while read -r oldrev newrev refname; do
    REFS+=("$refname")
    log "  ref: $refname ($oldrev -> $newrev)"
done
```

Git's `post-receive` hook receives one line per updated ref on stdin, formatted as:

```
<old-sha1> <new-sha1> <refname>
```

For example:
```
abc123 def456 refs/heads/main
000000 789abc refs/tags/v1.0.0
```

The `read -r` flag prevents backslash interpretation. Each ref name is accumulated in the `REFS` bash array for later use in notifications.

### Push Strategy

For each mirror remote, the script performs a `--mirror --force` push:

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

The `git push` flags are critical:

| Flag | Effect |
|------|--------|
| `--mirror` | Push all refs under `refs/` — branches, tags, notes, replace refs, everything. Also deletes remote refs that no longer exist locally. |
| `--force` | Force-update refs that have diverged. Ensures the mirror is an exact copy. |

The `2>&1` redirects stderr to stdout so both success and error messages are captured by `tee`. The `if` statement checks the exit code of the entire pipeline — if `git push` fails (non-zero exit), the remote is added to `FAILED_REMOTES`.

**Important**: The loop does **not** use `set -e` behavior for individual pushes because the `if` statement captures the exit code rather than triggering an immediate exit. This ensures all remotes are attempted even if some fail.

### Result Tracking

Two arrays track the outcome:

```bash
FAILED_REMOTES=()
SUCCEEDED_REMOTES=()
```

After the loop, a summary is logged:

```bash
log "--- Summary ---"
log "  Succeeded: ${SUCCEEDED_REMOTES[*]:-none}"
log "  Failed:    ${FAILED_REMOTES[*]:-none}"
```

The `${array[*]:-none}` syntax expands all array elements separated by spaces, or prints "none" if the array is empty.

### Failure Notification

When mirrors fail and `MIRROR_NOTIFY` is set, the script sends an email:

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

The notification includes:

- Which remotes failed (`FAILED_REMOTES`)
- The repository path (`$(pwd)`)
- Which refs were updated (`REFS`)
- Where to find detailed logs (`$MIRROR_LOG`)

The subject line uses the repository directory name: `[git-mirror] Push failure in project-tick.git`.

The `command -v mail &>/dev/null` check ensures the script doesn't crash if `mail` is not installed — it simply skips notification silently.

### Exit Behavior

```bash
if [[ ${#FAILED_REMOTES[@]} -gt 0 ]]; then
    log "=== Finished with errors ==="
    exit 1
fi

log "=== Finished successfully ==="
exit 0
```

| Condition | Exit Code | Meaning |
|-----------|-----------|---------|
| All remotes succeeded | `0` | Success — the pusher sees no error |
| One or more remotes failed | `1` | Failure — the pusher sees an error message |
| No remotes configured | `0` | No-op — silent success |

**Note**: A non-zero exit from `post-receive` does **not** reject the push (the refs are already updated). It only causes Git to display the hook's output as an error to the pusher. This alerts the developer that mirroring failed without rolling back their work.

---

## Supported Forge Targets

The script header documents four forge platforms with example remote URLs:

### GitHub

```bash
# SSH
git remote add github git@github.com:Project-Tick/Project-Tick.git

# HTTPS with token
git remote add github https://x-access-token:TOKEN@github.com/Project-Tick/Project-Tick.git
```

GitHub uses `x-access-token` as the username for personal access tokens and GitHub App installation tokens.

### GitLab

```bash
# SSH
git remote add gitlab git@gitlab.com:Project-Tick/Project-Tick.git

# HTTPS with token
git remote add gitlab https://oauth2:TOKEN@gitlab.com/Project-Tick/Project-Tick.git
```

GitLab uses `oauth2` as the username for personal access tokens with HTTPS.

### Codeberg

```bash
# SSH
git remote add codeberg git@codeberg.org:Project-Tick/Project-Tick.git

# HTTPS with token
git remote add codeberg https://TOKEN@codeberg.org/Project-Tick/Project-Tick.git
```

Codeberg (Gitea-based) accepts the token directly as the username with no password.

### SourceForge

```bash
# SSH only
git remote add sourceforge ssh://USERNAME@git.code.sf.net/p/project-tick/code
```

SourceForge uses a non-standard SSH URL format with a username prefix and a project-specific path structure.

---

## Authentication Methods

### SSH Key Authentication

SSH-based authentication requires:

1. An SSH keypair accessible to the user running the Git daemon
2. The public key registered on each forge platform
3. Correct SSH host key verification (or entries in `~/.ssh/known_hosts`)

For automated server-side usage, a dedicated deploy key is recommended:

```bash
# Generate a dedicated mirror key
ssh-keygen -t ed25519 -f ~/.ssh/mirror_key -N ""

# Configure SSH to use it for each host
cat >> ~/.ssh/config <<EOF
Host github.com
    IdentityFile ~/.ssh/mirror_key
Host gitlab.com
    IdentityFile ~/.ssh/mirror_key
Host codeberg.org
    IdentityFile ~/.ssh/mirror_key
EOF
```

### HTTPS Token Authentication

HTTPS authentication embeds the token in the remote URL. The token format varies by forge:

| Forge | URL Format | Token Type |
|-------|------------|------------|
| GitHub | `https://x-access-token:TOKEN@github.com/...` | Personal Access Token or App Installation Token |
| GitLab | `https://oauth2:TOKEN@gitlab.com/...` | Personal Access Token |
| Codeberg | `https://TOKEN@codeberg.org/...` | Application Token |

**Security warning**: Tokens embedded in remote URLs are stored in the Git config file of the bare repository. Ensure the repository directory has restrictive permissions (`chmod 700`).

---

## Environment Variables

The script supports three environment variables for runtime configuration:

### `MIRROR_REMOTES`

```bash
MIRROR_REMOTES="${MIRROR_REMOTES:-}"
```

- **Type**: Space-separated string of git remote names
- **Default**: Empty (triggers auto-detection of all non-`origin` remotes)
- **Example**: `MIRROR_REMOTES="github gitlab codeberg"`
- **Use case**: Restrict mirroring to specific remotes, e.g., push to GitHub and GitLab but skip Codeberg temporarily

### `MIRROR_LOG`

```bash
MIRROR_LOG="${MIRROR_LOG:-/var/log/git-mirror.log}"
```

- **Type**: Filesystem path
- **Default**: `/var/log/git-mirror.log`
- **Example**: `MIRROR_LOG=/var/log/project-tick/mirror.log`
- **Requirements**: The directory must exist and be writable by the user running the hook. If not writable, the script falls back to stdout-only logging.

### `MIRROR_NOTIFY`

```bash
"${MIRROR_NOTIFY:-}"
```

- **Type**: Email address string
- **Default**: Empty (notifications disabled)
- **Example**: `MIRROR_NOTIFY=admin@project-tick.org`
- **Requirements**: The `mail` command must be available on the system. If `mail` is not installed, the notification is silently skipped.

---

## Installation Guide

### Step 1: Locate the Bare Repository

```bash
# The bare repository is typically at:
cd /srv/git/project-tick.git
# or
cd /var/lib/gitolite/repositories/project-tick.git
```

### Step 2: Copy the Hook Script

```bash
cp /path/to/Project-Tick/hooks/post-receive hooks/post-receive
chmod +x hooks/post-receive
```

### Step 3: Configure Mirror Remotes

```bash
git remote add github  git@github.com:Project-Tick/Project-Tick.git
git remote add gitlab  git@gitlab.com:Project-Tick/Project-Tick.git
git remote add codeberg git@codeberg.org:Project-Tick/Project-Tick.git
```

### Step 4: Verify Remote Configuration

```bash
git remote -v
# github    git@github.com:Project-Tick/Project-Tick.git (push)
# gitlab    git@gitlab.com:Project-Tick/Project-Tick.git (push)
# codeberg  git@codeberg.org:Project-Tick/Project-Tick.git (push)
# origin    (local bare repo — no push URL)
```

### Step 5: Set Up Logging

```bash
sudo mkdir -p /var/log/
sudo touch /var/log/git-mirror.log
sudo chown git:git /var/log/git-mirror.log
```

### Step 6: (Optional) Configure Notifications

```bash
# Set in the shell environment of the user running the git daemon
export MIRROR_NOTIFY="admin@project-tick.org"
```

### Step 7: Test the Hook

```bash
echo "0000000000000000000000000000000000000000 $(git rev-parse HEAD) refs/heads/main" | hooks/post-receive
```

---

## Directory Layout

```
Project-Tick/
├── hooks/
│   └── post-receive              # The mirror hook script (source copy)
│
├── docs/handbook/hooks/
│   ├── overview.md               # This document
│   ├── post-receive-hook.md      # Deep-dive into the post-receive script
│   ├── mirror-configuration.md   # Mirror setup and forge configuration
│   ├── logging-system.md         # Logging internals
│   └── notification-system.md    # Failure notification system
│
└── /path/to/project-tick.git/    # Deployed bare repository
    └── hooks/
        └── post-receive          # Deployed copy (executable)
```

---

## Operational Flow Diagram

```
Developer pushes to canonical repo
        │
        ▼
  Git updates refs in bare repo
        │
        ▼
  post-receive hook is invoked
        │
        ▼
  Read stdin: old-sha, new-sha, refname
        │
        ▼
  Auto-detect mirror remotes
  (all remotes except "origin")
        │
        ├── No remotes? ──► exit 0 (silent)
        │
        ▼
  For each remote:
    git push --mirror --force $remote
        │
        ├── Success ──► add to SUCCEEDED_REMOTES
        │
        └── Failure ──► add to FAILED_REMOTES
                │
                ▼
          MIRROR_NOTIFY set?
                │
                ├── Yes + mail available ──► send email
                │
                └── No ──► skip
        │
        ▼
  Log summary
        │
        ├── Any failures? ──► exit 1
        │
        └── All ok? ──► exit 0
```

---

## Interaction with Other Project-Tick Components

### cgit Integration

The Project-Tick monorepo includes `cgit/` — a web frontend for Git repositories. The `post-receive` mirroring hook complements cgit by ensuring that the repositories displayed on the cgit web interface are kept in sync across multiple forges.

The `cgit/contrib/hooks/post-receive.agefile` hook (a separate, cgit-specific hook) updates the `info/web/last-modified` file for cgit's cache invalidation. In a multi-hook setup, both hooks can be combined using a wrapper script.

### lefthook Integration

The `lefthook.yml` at the repository root configures client-side hooks for the development workflow. This is complementary to the server-side `post-receive` hook — lefthook manages pre-commit and pre-push checks locally, while `post-receive` manages post-push mirroring on the server.

### CI Pipeline

The `ci/` directory contains CI configuration. The mirror hook runs independently of CI — it triggers on the bare repository while CI typically triggers on the forge platforms that receive the mirrored pushes.

---

## Troubleshooting Common Issues

### Hook Not Executing

```bash
# Check permissions
ls -la hooks/post-receive
# Must show: -rwxr-xr-x or similar with execute bit

# Fix permissions
chmod +x hooks/post-receive
```

### "No mirror remotes configured"

```bash
# Verify remotes exist
git remote -v

# If empty, add remotes:
git remote add github git@github.com:Project-Tick/Project-Tick.git
```

### SSH Authentication Failures

```bash
# Test SSH connectivity
ssh -T git@github.com
ssh -T git@gitlab.com
ssh -T git@codeberg.org

# Check SSH agent
ssh-add -l
```

### Log File Not Writable

```bash
# Check permissions
ls -la /var/log/git-mirror.log

# Create with correct ownership
sudo touch /var/log/git-mirror.log
sudo chown $(whoami) /var/log/git-mirror.log
```

### Push Rejected by Remote

```bash
# Check if the remote repository exists
# Check if the token/key has push permissions
# Check if branch protection rules block --force pushes
```

---

## Security Considerations

1. **Token storage** — HTTPS tokens embedded in remote URLs are stored in plain text in the git config. Restrict access to the bare repository directory.
2. **SSH keys** — Use dedicated deploy keys with minimal permissions (push-only, no admin).
3. **Log file contents** — The log file may contain ref names and remote names but should not contain credentials. However, restrict access to logs as ref names may be sensitive.
4. **`set -euo pipefail`** — The strict bash mode prevents silent failures and unset variable references that could lead to unexpected behavior.
5. **`--force` flag** — The `--force` flag overwrites remote refs unconditionally. This is intentional for mirroring but means the canonical repo must be protected against unauthorized pushes.

---

## Related Documentation

- [post-receive-hook.md](post-receive-hook.md) — Line-by-line analysis of the post-receive script
- [mirror-configuration.md](mirror-configuration.md) — Detailed mirror remote setup guide
- [logging-system.md](logging-system.md) — Logging system internals
- [notification-system.md](notification-system.md) — Email notification system
