# Mirror Configuration

## Table of Contents

- [Introduction](#introduction)
- [How Git Mirroring Works](#how-git-mirroring-works)
  - [Push Mirroring vs Fetch Mirroring](#push-mirroring-vs-fetch-mirroring)
  - [Ref Namespaces Synchronized](#ref-namespaces-synchronized)
  - [The --mirror Flag Internals](#the---mirror-flag-internals)
  - [The --force Flag and Divergent History](#the---force-flag-and-divergent-history)
- [Mirror Remote Configuration](#mirror-remote-configuration)
  - [Adding a Mirror Remote](#adding-a-mirror-remote)
  - [Listing Configured Remotes](#listing-configured-remotes)
  - [Modifying a Remote URL](#modifying-a-remote-url)
  - [Removing a Mirror Remote](#removing-a-mirror-remote)
- [Supported Protocols](#supported-protocols)
  - [SSH Protocol](#ssh-protocol)
  - [HTTPS Protocol](#https-protocol)
  - [Git Protocol](#git-protocol)
  - [Local Path Protocol](#local-path-protocol)
- [Forge-Specific Configuration](#forge-specific-configuration)
  - [GitHub](#github)
  - [GitLab](#gitlab)
  - [Codeberg](#codeberg)
  - [SourceForge](#sourceforge)
  - [Bitbucket](#bitbucket)
  - [Gitea (Self-Hosted)](#gitea-self-hosted)
- [Authentication Setup](#authentication-setup)
  - [SSH Key Authentication](#ssh-key-authentication)
  - [HTTPS Token Authentication](#https-token-authentication)
  - [SSH Config for Multiple Keys](#ssh-config-for-multiple-keys)
  - [Token Scopes and Permissions](#token-scopes-and-permissions)
- [The MIRROR_REMOTES Variable](#the-mirror_remotes-variable)
  - [Auto-Detection Mode](#auto-detection-mode)
  - [Explicit Remote List](#explicit-remote-list)
  - [Excluding Specific Remotes](#excluding-specific-remotes)
- [Git Config File Format](#git-config-file-format)
- [Multi-Repository Mirroring](#multi-repository-mirroring)
- [Troubleshooting Mirror Issues](#troubleshooting-mirror-issues)

---

## Introduction

The Project-Tick `post-receive` hook (`hooks/post-receive`) mirrors the canonical bare repository to multiple forge platforms. This document covers the configuration of mirror remotes — how to set them up, what protocols and authentication methods are supported, and how the hook discovers and uses them.

The mirror push is triggered by the following line in the hook:

```bash
git push --mirror --force "$remote"
```

Everything in this document revolves around configuring the `$remote` targets that this command pushes to.

---

## How Git Mirroring Works

### Push Mirroring vs Fetch Mirroring

Git supports two mirroring directions:

| Type | Command | Direction |
|------|---------|-----------|
| Push mirror | `git push --mirror <remote>` | Local → Remote |
| Fetch mirror | `git clone --mirror <url>` | Remote → Local |

The Project-Tick hook uses **push mirroring** — the canonical repository pushes its refs outward to each forge. This is the active/upstream pattern: changes flow from one source to many targets.

The opposite approach, fetch mirroring, would require each forge to periodically pull from the canonical repo. Push mirroring is preferred because it provides immediate synchronization without polling latency.

### Ref Namespaces Synchronized

When `git push --mirror` executes, it synchronizes **all** refs under the `refs/` hierarchy:

| Ref Namespace | Contents | Example |
|---------------|----------|---------|
| `refs/heads/*` | Branches | `refs/heads/main`, `refs/heads/feature/x` |
| `refs/tags/*` | Tags | `refs/tags/v1.0.0`, `refs/tags/v2.0.0-rc1` |
| `refs/notes/*` | Git notes | `refs/notes/commits` |
| `refs/replace/*` | Replacement objects | `refs/replace/<sha>` |
| `refs/meta/*` | Metadata refs (Gerrit) | `refs/meta/config` |

Notably, `--mirror` also **deletes** remote refs that no longer exist locally. If a branch `feature/old` is deleted in the canonical repo, the mirror push removes it from all mirrors.

### The --mirror Flag Internals

Under the hood, `git push --mirror` is equivalent to:

```bash
git push --force --prune <remote> 'refs/*:refs/*'
```

This refspec (`refs/*:refs/*`) maps every local ref to the same-named remote ref. The `--prune` flag deletes remote refs that have no local counterpart.

### The --force Flag and Divergent History

The `--force` flag in the hook's `git push --mirror --force` is redundant with `--mirror` (which implies force), but it's included explicitly for clarity. It handles the case where a ref on the canonical repo has been rewritten (e.g., via `git push --force` or `git rebase`), which would otherwise be rejected as a non-fast-forward update:

```
 ! [rejected]        main -> main (non-fast-forward)
```

With `--force`, the mirror is overwritten to match the canonical state exactly, even if that means losing remote-only history.

---

## Mirror Remote Configuration

### Adding a Mirror Remote

From within the bare repository:

```bash
cd /path/to/project-tick.git
git remote add <name> <url>
```

The `<name>` can be any valid git remote name. The hook auto-discovers all remotes that aren't named `origin`:

```bash
MIRROR_REMOTES=$(git remote | grep -v '^origin$' || true)
```

**Convention**: Use the forge name as the remote name:

```bash
git remote add github   git@github.com:Project-Tick/Project-Tick.git
git remote add gitlab   git@gitlab.com:Project-Tick/Project-Tick.git
git remote add codeberg git@codeberg.org:Project-Tick/Project-Tick.git
```

### Listing Configured Remotes

```bash
git remote -v
```

Output:
```
codeberg   git@codeberg.org:Project-Tick/Project-Tick.git (fetch)
codeberg   git@codeberg.org:Project-Tick/Project-Tick.git (push)
github     git@github.com:Project-Tick/Project-Tick.git (fetch)
github     git@github.com:Project-Tick/Project-Tick.git (push)
gitlab     git@gitlab.com:Project-Tick/Project-Tick.git (fetch)
gitlab     git@gitlab.com:Project-Tick/Project-Tick.git (push)
origin     /srv/git/project-tick.git (fetch)
origin     /srv/git/project-tick.git (push)
```

### Modifying a Remote URL

```bash
git remote set-url github https://x-access-token:NEW_TOKEN@github.com/Project-Tick/Project-Tick.git
```

### Removing a Mirror Remote

```bash
git remote remove codeberg
```

The hook will no longer push to Codeberg on subsequent pushes. This is the recommended way to temporarily or permanently disable a mirror.

---

## Supported Protocols

### SSH Protocol

**URL format**:
```
git@<host>:<owner>/<repo>.git
ssh://<user>@<host>/<path>
```

**Examples**:
```bash
git remote add github  git@github.com:Project-Tick/Project-Tick.git
git remote add sourceforge ssh://USERNAME@git.code.sf.net/p/project-tick/code
```

**Characteristics**:
- Authenticated via SSH keypair
- Supports key-based automation
- Port 22 by default (or custom via `ssh://host:port/path`)
- Requires public key registration on the forge

**Best for**: Server-side automation where SSH keys can be managed securely.

### HTTPS Protocol

**URL format**:
```
https://<user>:<token>@<host>/<owner>/<repo>.git
```

**Examples**:
```bash
git remote add github  https://x-access-token:TOKEN@github.com/Project-Tick/Project-Tick.git
git remote add gitlab  https://oauth2:TOKEN@gitlab.com/Project-Tick/Project-Tick.git
git remote add codeberg https://TOKEN@codeberg.org/Project-Tick/Project-Tick.git
```

**Characteristics**:
- Token embedded in URL (stored in git config)
- Works behind HTTP proxies
- Port 443 by default
- No SSH key management needed

**Best for**: Environments where SSH is blocked or key management is impractical.

### Git Protocol

**URL format**:
```
git://<host>/<path>
```

**Characteristics**:
- Read-only — **cannot** be used for mirroring
- Unauthenticated
- Port 9418

Not suitable for the mirror hook.

### Local Path Protocol

**URL format**:
```
/path/to/repo.git
file:///path/to/repo.git
```

**Characteristics**:
- No network involved
- Useful for local backup mirrors
- Fast — uses hardlinks when possible

**Example**:
```bash
git remote add backup /mnt/backup/project-tick.git
```

---

## Forge-Specific Configuration

### GitHub

**SSH remote**:
```bash
git remote add github git@github.com:Project-Tick/Project-Tick.git
```

**HTTPS remote**:
```bash
git remote add github https://x-access-token:ghp_XXXX@github.com/Project-Tick/Project-Tick.git
```

**Token format**: GitHub Personal Access Tokens start with `ghp_` (classic) or `github_pat_` (fine-grained).

**Required token scopes**:
- `repo` (full control of private repositories) — for classic tokens
- Repository permissions → Contents → Read and Write — for fine-grained tokens

**GitHub-specific considerations**:
- GitHub may reject pushes that include non-standard refs (e.g., `refs/pull/*` or `refs/keep-around/*`). Since these refs don't exist in the canonical repo, this is typically not an issue.
- Branch protection rules may block `--force` pushes. Ensure the mirror token has admin access or that branch protection allows force pushes from the mirror user.
- GitHub has a push size limit of 2 GB per push. Large mirror pushes may need to be split.

### GitLab

**SSH remote**:
```bash
git remote add gitlab git@gitlab.com:Project-Tick/Project-Tick.git
```

**HTTPS remote**:
```bash
git remote add gitlab https://oauth2:glpat-XXXX@gitlab.com/Project-Tick/Project-Tick.git
```

**Token format**: GitLab Personal Access Tokens start with `glpat-`.

**Required token scopes**:
- `write_repository` — for push access

**GitLab-specific considerations**:
- GitLab supports built-in repository mirroring (Settings → Repository → Mirroring). This is an alternative to the hook-based approach but requires GitLab Premium for push mirroring.
- Protected branches/tags may reject force pushes. Configure the mirror user as a Maintainer with force-push permissions.

### Codeberg

**SSH remote**:
```bash
git remote add codeberg git@codeberg.org:Project-Tick/Project-Tick.git
```

**HTTPS remote**:
```bash
git remote add codeberg https://TOKEN@codeberg.org/Project-Tick/Project-Tick.git
```

**Token format**: Codeberg (Gitea) uses application tokens without a specific prefix.

**Required token permissions**:
- Repository → Write

**Codeberg-specific considerations**:
- Codeberg runs Gitea/Forgejo. Token authentication uses the token as the username with no password (or any dummy password).
- Codeberg also supports Gitea's built-in mirror feature.

### SourceForge

**SSH remote** (SSH only — no HTTPS push support):
```bash
git remote add sourceforge ssh://USERNAME@git.code.sf.net/p/project-tick/code
```

**SourceForge-specific considerations**:
- SourceForge uses a non-standard URL format: `ssh://USERNAME@git.code.sf.net/p/<project>/<mount>`
- The `USERNAME` is the SourceForge account username
- SSH key must be registered at https://sourceforge.net/auth/shell_services
- SourceForge may have rate limits on Git operations

### Bitbucket

**SSH remote**:
```bash
git remote add bitbucket git@bitbucket.org:Project-Tick/Project-Tick.git
```

**HTTPS remote**:
```bash
git remote add bitbucket https://USERNAME:APP_PASSWORD@bitbucket.org/Project-Tick/Project-Tick.git
```

**Bitbucket-specific considerations**:
- Bitbucket uses App Passwords (not PATs) for HTTPS authentication
- The username is the Bitbucket account username (not email)

### Gitea (Self-Hosted)

**SSH remote**:
```bash
git remote add gitea git@gitea.example.com:Project-Tick/Project-Tick.git
```

**HTTPS remote**:
```bash
git remote add gitea https://TOKEN@gitea.example.com/Project-Tick/Project-Tick.git
```

**Considerations**:
- Self-hosted Gitea instances may use custom SSH ports: `ssh://git@gitea.example.com:2222/Project-Tick/Project-Tick.git`
- Self-signed TLS certificates require `git config http.sslVerify false` on the bare repo (not recommended for production)

---

## Authentication Setup

### SSH Key Authentication

Generate a dedicated mirror key:

```bash
ssh-keygen -t ed25519 -C "project-tick-mirror" -f ~/.ssh/mirror_key -N ""
```

Register the public key (`~/.ssh/mirror_key.pub`) as a deploy key on each forge:

| Forge | Registration Path |
|-------|------------------|
| GitHub | Repository → Settings → Deploy keys |
| GitLab | Repository → Settings → Repository → Deploy keys |
| Codeberg | Repository → Settings → Deploy Keys |
| SourceForge | Account → SSH Settings |

### HTTPS Token Authentication

Tokens are embedded directly in the remote URL as documented in each forge's section above. The token is stored in the bare repository's git config file:

```bash
cat /path/to/project-tick.git/config
```

```ini
[remote "github"]
    url = https://x-access-token:ghp_XXXX@github.com/Project-Tick/Project-Tick.git
    fetch = +refs/*:refs/remotes/github/*
```

**Security note**: Ensure the config file has restrictive permissions:

```bash
chmod 600 /path/to/project-tick.git/config
```

### SSH Config for Multiple Keys

When different forges require different SSH keys, use `~/.ssh/config`:

```
Host github.com
    HostName github.com
    User git
    IdentityFile ~/.ssh/github_mirror_key
    IdentitiesOnly yes

Host gitlab.com
    HostName gitlab.com
    User git
    IdentityFile ~/.ssh/gitlab_mirror_key
    IdentitiesOnly yes

Host codeberg.org
    HostName codeberg.org
    User git
    IdentityFile ~/.ssh/codeberg_mirror_key
    IdentitiesOnly yes

Host git.code.sf.net
    HostName git.code.sf.net
    User USERNAME
    IdentityFile ~/.ssh/sourceforge_mirror_key
    IdentitiesOnly yes
```

The `IdentitiesOnly yes` directive ensures only the specified key is offered, preventing SSH from trying all loaded keys.

### Token Scopes and Permissions

Minimum required permissions for each forge:

| Forge | Scope/Permission | Allows |
|-------|-----------------|--------|
| GitHub (classic PAT) | `repo` | Push to public and private repos |
| GitHub (fine-grained) | Contents: Read and Write | Push to the specific repo |
| GitLab | `write_repository` | Push to the repo |
| Codeberg | Repository: Write | Push to the repo |
| Bitbucket | Repositories: Write | Push to the repo |

**Principle of least privilege**: Use fine-grained tokens scoped to a single repository when possible. Avoid tokens with admin or organizational permissions.

---

## The MIRROR_REMOTES Variable

### Auto-Detection Mode

When `MIRROR_REMOTES` is not set (the default), the hook auto-detects remotes:

```bash
MIRROR_REMOTES="${MIRROR_REMOTES:-}"

if [[ -z "$MIRROR_REMOTES" ]]; then
    MIRROR_REMOTES=$(git remote | grep -v '^origin$' || true)
fi
```

This discovers all remotes except `origin`. The assumption is:
- `origin` refers to the canonical repo itself (if configured)
- Everything else is a mirror target

### Explicit Remote List

Override auto-detection by setting `MIRROR_REMOTES`:

```bash
export MIRROR_REMOTES="github gitlab"
```

This restricts mirroring to only the named remotes, ignoring others like `codeberg` or `sourceforge`. Useful for:
- Temporarily disabling a problematic mirror
- Phased rollouts of new mirrors
- Testing a single mirror

### Excluding Specific Remotes

There is no built-in exclude mechanism, but you can achieve it by explicitly listing the desired remotes:

```bash
# Mirror to everything except sourceforge
export MIRROR_REMOTES="github gitlab codeberg"
```

Alternatively, modify the auto-detection grep to exclude additional patterns:

```bash
MIRROR_REMOTES=$(git remote | grep -v -e '^origin$' -e '^sourceforge$' || true)
```

---

## Git Config File Format

The mirror remotes are stored in the bare repository's `config` file. The relevant sections look like:

```ini
[remote "origin"]
    url = /srv/git/project-tick.git
    fetch = +refs/*:refs/remotes/origin/*

[remote "github"]
    url = git@github.com:Project-Tick/Project-Tick.git
    fetch = +refs/*:refs/remotes/github/*

[remote "gitlab"]
    url = git@gitlab.com:Project-Tick/Project-Tick.git
    fetch = +refs/*:refs/remotes/gitlab/*

[remote "codeberg"]
    url = git@codeberg.org:Project-Tick/Project-Tick.git
    fetch = +refs/*:refs/remotes/codeberg/*

[remote "sourceforge"]
    url = ssh://USERNAME@git.code.sf.net/p/project-tick/code
    fetch = +refs/*:refs/remotes/sourceforge/*
```

You can edit this file directly instead of using `git remote add`:

```bash
vim /path/to/project-tick.git/config
```

However, `git remote add` is preferred because it ensures correct syntax and creates the appropriate fetch refspec.

---

## Multi-Repository Mirroring

If Project-Tick manages multiple bare repositories (e.g., separate repos for subprojects), the same hook script can be deployed to each:

```bash
for repo in /srv/git/*.git; do
    cp hooks/post-receive "$repo/hooks/post-receive"
    chmod +x "$repo/hooks/post-receive"
done
```

Each repository needs its own mirror remotes configured:

```bash
cd /srv/git/project-tick.git
git remote add github git@github.com:Project-Tick/Project-Tick.git

cd /srv/git/sub-project.git
git remote add github git@github.com:Project-Tick/sub-project.git
```

The hook's auto-detection ensures each repository mirrors to its own set of remotes without shared configuration.

---

## Troubleshooting Mirror Issues

### Remote URL Typo

**Symptom**: `fatal: 'github' does not appear to be a git repository`

**Fix**: Verify the remote URL:
```bash
git remote get-url github
```

### SSH Host Key Verification Failed

**Symptom**: `Host key verification failed.`

**Fix**: Add the host key to known_hosts:
```bash
ssh-keyscan github.com >> ~/.ssh/known_hosts
ssh-keyscan gitlab.com >> ~/.ssh/known_hosts
ssh-keyscan codeberg.org >> ~/.ssh/known_hosts
ssh-keyscan git.code.sf.net >> ~/.ssh/known_hosts
```

### HTTPS Token Expired

**Symptom**: `fatal: Authentication failed for 'https://...'`

**Fix**: Update the token in the remote URL:
```bash
git remote set-url github https://x-access-token:NEW_TOKEN@github.com/Project-Tick/Project-Tick.git
```

### Force Push Rejected by Branch Protection

**Symptom**: `! [remote rejected] main -> main (protected branch hook declined)`

**Fix**: On the forge, either:
1. Grant the mirror user admin/bypass permissions
2. Add the mirror user/key to the force-push allowlist
3. Disable branch protection for the mirror repository (if appropriate)

### Push Size Limit Exceeded

**Symptom**: `fatal: the remote end hung up unexpectedly` (during large pushes)

**Fix**: Increase Git's buffer sizes:
```bash
git config http.postBuffer 524288000  # 500 MB
```

Or perform an initial manual push before enabling automated mirroring.

### SSH Agent Not Available

**Symptom**: `Permission denied (publickey).`

**Fix**: Ensure the SSH key is loaded or use `IdentityFile` in SSH config:
```bash
ssh-add ~/.ssh/mirror_key
# or configure ~/.ssh/config with IdentityFile
```

### Network Timeout

**Symptom**: `fatal: unable to access '...': Failed to connect to ... port 443: Connection timed out`

**Fix**: Check network connectivity, proxy settings, and firewall rules. Consider setting a git timeout:
```bash
git config http.lowSpeedLimit 1000
git config http.lowSpeedTime 300
```
