# Project Tick — Contributing Guide

## Overview

Project Tick welcomes contributions from the community. This guide covers the
full contribution lifecycle — from setting up your environment to getting your
changes merged. It applies to all sub-projects within the monorepo.

All contributions are subject to the Project Tick Contributor License Agreement
(CLA), the Developer Certificate of Origin (DCO), and the project's Code of
Conduct.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [AI Policy](#ai-policy)
3. [Contributor License Agreement](#contributor-license-agreement)
4. [Developer Certificate of Origin](#developer-certificate-of-origin)
5. [Commit Message Format](#commit-message-format)
6. [Branch Naming](#branch-naming)
7. [PR Workflow](#pr-workflow)
8. [Code Review Process](#code-review-process)
9. [Issue Templates](#issue-templates)
10. [PR Requirements Checklist](#pr-requirements-checklist)
11. [What Not to Do](#what-not-to-do)
12. [Documentation](#documentation)
13. [Contact](#contact)

---

## Quick Start

```bash
# 1. Fork and clone
git clone --recursive https://github.com/YOUR_USERNAME/Project-Tick.git
cd Project-Tick

# 2. Create a branch
git checkout -b feat/my-change

# 3. Make changes, format, and lint
clang-format -i changed_files.cpp
reuse lint

# 4. Commit with sign-off
git commit -s -a -m "feat(meshmc): add new feature"

# 5. Push and open a PR
git push origin feat/my-change
```

---

## AI Policy

Project Tick has strict rules regarding generative AI usage. This policy is
adapted from matplotlib's contributing guide and the Linux Kernel policy guide.

### Rules

1. **Do not post raw AI output** as comments on GitHub or the project's Discord
   server. Such comments are typically formulaic and low-quality.

2. **If you use AI tools** to develop code or documentation, you must:
   - Fully understand the proposed changes
   - Be able to explain why they are the correct approach
   - Add personal value based on your own competency

3. **AI-generated low-value contributions will be rejected.** Taking input,
   feeding it to an AI, and posting the result without adding value is not
   acceptable.

### Signed-off-by and AI

**AI agents MUST NOT add `Signed-off-by` tags.** Only humans can legally
certify the Developer Certificate of Origin. The human submitter is responsible
for:

- Reviewing all AI-generated code
- Ensuring compliance with licensing requirements
- Adding their own `Signed-off-by` tag
- Taking full responsibility for the contribution

### AI Attribution

When AI tools contribute to development, include an `Assisted-by` tag in the
commit message:

```
Assisted-by: AGENT_NAME:MODEL_VERSION [TOOL1] [TOOL2]
```

Where:
- `AGENT_NAME` — Name of the AI tool or framework
- `MODEL_VERSION` — Specific model version used
- `[TOOL1] [TOOL2]` — Optional specialized analysis tools (e.g., coccinelle,
  sparse, smatch, clang-tidy)

Basic development tools (git, gcc, make, editors) should **not** be listed.

Example:

```
feat(meshmc): optimize chunk loading algorithm

Improved chunk loading performance by 40% using spatial hashing.

Signed-off-by: Jane Developer <jane@example.com>
Assisted-by: Claude:claude-3-opus coccinelle sparse
```

---

## Contributor License Agreement

By submitting a contribution, you agree to the **Project Tick Contributor
License Agreement (CLA)**.

The CLA ensures that:

- You have the legal right to submit the contribution
- The contribution does not knowingly infringe third-party rights
- Project Tick may distribute the contribution under the applicable license(s)
- Long-term governance and license consistency can be maintained

The CLA applies to **all intentional contributions**, including:
- Source code
- Documentation
- Tests
- Data
- Media assets
- Configuration files

The full CLA text is available at:
<https://projecttick.org/licenses/PT-CLA-2.0.txt>

**If you do not agree to the CLA, do not submit contributions.**

---

## Developer Certificate of Origin

Every commit in Project Tick must include a DCO sign-off line. The sign-off
certifies that you wrote the code or have the right to submit it under the
project's licenses.

### How to Sign Off

Add the `-s` flag to `git commit`:

```bash
git commit -s -a
```

This appends the following line to your commit message:

```
Signed-off-by: Your Name <your.email@example.com>
```

### Retroactive Sign-Off

If you forgot to sign off, you can retroactively sign all commits in your
branch:

```bash
git rebase --signoff develop
git push --force
```

### DCO Bot Enforcement

A DCO bot automatically checks every PR. PRs missing sign-off will be
labeled and blocked from merging. The bot configuration
(`.github/dco.yml`) does not allow remediation commits — every commit
in the PR must have a sign-off.

```yaml
# .github/dco.yml
allowRemediationCommits:
  individual: false
```

### Important Distinction

**Signing** (GPG/SSH signatures) and **signing-off** (DCO `Signed-off-by`) are
two different things. The DCO sign-off is the minimum requirement. GPG signing
is recommended but not required.

---

## Commit Message Format

Project Tick uses [Conventional Commits](https://www.conventionalcommits.org/)
format. The CI system validates commit messages via
`ci/github-script/lint-commits.js`.

### Format

```
type(scope): short description

Optional longer explanation of what changed and why.

Signed-off-by: Your Name <your.email@example.com>
```

### Types

| Type | Description |
|------|-------------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `style` | Formatting, whitespace (no code change) |
| `refactor` | Code restructuring (no feature/fix) |
| `perf` | Performance improvement |
| `test` | Adding or updating tests |
| `build` | Build system or dependency changes |
| `ci` | CI configuration changes |
| `chore` | Maintenance tasks |
| `revert` | Reverting a previous commit |

### Scopes

The scope identifies which sub-project is affected:

| Scope | Sub-Project |
|-------|-------------|
| `meshmc` | MeshMC launcher |
| `mnv` | MNV text editor |
| `cgit` | cgit web interface |
| `neozip` | NeoZip compression library |
| `json4cpp` | Json4C++ JSON library |
| `tomlplusplus` | toml++ TOML library |
| `libnbt` | libnbt++ NBT library |
| `cmark` | cmark Markdown library |
| `genqrcode` | GenQRCode QR library |
| `forgewrapper` | ForgeWrapper Java shim |
| `corebinutils` | CoreBinUtils BSD ports |
| `meta` | Metadata generator |
| `tickborg` | tickborg CI bot |
| `ci` | CI infrastructure |
| `docker` | images4docker |
| `docs` | Documentation |

For changes spanning the component's sub-structure, add a nested scope:

```
projtlauncher(fix): fix crash on startup with invalid config
```

### Examples

```
feat(meshmc): add chunk loading optimization
fix(neozip): handle empty archives in inflate
docs(cmark): fix API reference typo
ci(json4cpp): update build matrix for ARM64
build(tomlplusplus): bump meson minimum to 0.60
refactor(corebinutils): simplify ls output formatting
test(libnbt): add round-trip test for compressed NBT
chore(meta): update poetry.lock
```

### tickborg Integration

The tickborg CI bot reads commit scopes to determine which sub-projects
to build:

| Commit Message | Auto-Build |
|---------------|------------|
| `feat(meshmc): add chunk loading` | meshmc |
| `cmark: fix buffer overflow` | cmark |
| `fix(neozip): handle empty archives` | neozip |
| `chore(ci): update workflow` | (CI changes only) |

---

## Branch Naming

Use the following branch name prefixes:

| Prefix | Purpose | Example |
|--------|---------|---------|
| `feature-*` or `feat/*` | New features | `feature-chunk-loading` |
| `fix-*` or `fix/*` | Bug fixes | `fix-crash-on-startup` |
| `backport-*` | Cherry-picks to release | `backport-7.0-fix-123` |
| `revert-*` | Reverted changes | `revert-pr-456` |
| `wip-*` or `wip/*` | Work in progress | `wip-new-ui` |

Development branches managed by maintainers:

| Branch | Purpose |
|--------|---------|
| `master` | Main development branch |
| `release-X.Y` | Release stabilization (e.g., `release-7.0`) |
| `staging-*` | Pre-release staging |
| `staging-next-*` | Next staging cycle |

### WIP Convention

If a PR title begins with `WIP:` or contains `[WIP]`, the tickborg bot
will **not** automatically build its affected projects. This lets you
push incomplete work for early review without triggering full CI.

---

## PR Workflow

### Step-by-Step

1. **Fork** the repository on GitHub

2. **Clone** your fork with submodules:
   ```bash
   git clone --recursive https://github.com/YOUR_USERNAME/Project-Tick.git
   ```

3. **Set up upstream remote:**
   ```bash
   git remote add upstream https://github.com/Project-Tick/Project-Tick.git
   ```

4. **Create a feature branch:**
   ```bash
   git fetch upstream
   git checkout -b feature/my-change upstream/master
   ```

5. **Develop your changes:**
   - Write code
   - Add tests for new functionality
   - Update documentation if needed
   - Run clang-format on changed C/C++ files
   - Check REUSE compliance

6. **Commit with sign-off:**
   ```bash
   git add -A
   git commit -s -m "feat(scope): description"
   ```

7. **Push to your fork:**
   ```bash
   git push origin feature/my-change
   ```

8. **Open a PR** against `master` on the upstream repository

9. **Fill in the PR template** — The template reminds you to:
   - Sign off commits
   - Sign the CLA
   - Provide a clear description

### Keeping Your Branch Updated

```bash
git fetch upstream
git rebase upstream/master
git push --force-with-lease origin feature/my-change
```

---

## Code Review Process

### Automated Checks

Every PR goes through automated CI:

1. **Gate job** — Event classification and change detection
2. **Commit lint** — Validates Conventional Commits format
3. **Formatting check** — treefmt validates code style
4. **CODEOWNERS validation** — Ensures proper ownership rules
5. **Per-project CI** — Builds and tests affected sub-projects
6. **CodeQL analysis** — Security scanning (for meshmc, mnv, neozip)
7. **DCO check** — Verifies all commits are signed off

### Maintainer Review

After automated checks pass:

1. A maintainer reviews the code for:
   - Correctness
   - Design and architecture fit
   - Test coverage
   - Documentation completeness
   - License compliance

2. The maintainer may request changes by:
   - Leaving inline comments
   - Requesting specific modifications
   - Asking clarifying questions

3. Address all feedback by pushing additional commits or amending existing
   ones. Sign off every commit.

4. Once approved, the maintainer merges the PR.

### Review Routing

The `CODEOWNERS` file routes reviews automatically. All paths are currently
owned by `@YongDo-Hyun`, covering:

- `.github/` — Actions, templates, workflows
- `archived/` — All archived sub-projects
- `cgit/` — Including contrib, filters, tests
- `cmark/` — Including all subdirectories
- `corebinutils/` — All utility directories
- Every other sub-project directory

---

## Issue Templates

Project Tick provides structured issue templates in `.github/ISSUE_TEMPLATE/`:

### Bug Report (`bug_report.yml`)

Fields:
- **Operating System** — Windows, macOS, Linux, Other (multi-select)
- **Version of MeshMC** — Text field for version number
- Steps to reproduce
- Expected vs actual behavior
- Logs/crash reports

Before filing a bug, check:
- The [FAQ](https://github.com/Project-Tick/MeshMC/wiki/FAQ)
- That the bug is not caused by Minecraft or mods
- That the issue hasn't been reported before

### Suggestion (`suggestion.yml`)

For feature requests and improvements.

### RFC (`rfc.yml`)

For larger architectural proposals that need discussion before implementation.

### Configuration (`config.yml`)

Controls which templates appear and provides links to external resources (e.g.,
Discord for general help questions).

---

## PR Requirements Checklist

Before submitting a PR, verify:

- [ ] Code compiles without warnings
- [ ] clang-format applied to changed C/C++ files
- [ ] All existing tests pass
- [ ] New tests added for new functionality
- [ ] All commits signed off (`git commit -s`)
- [ ] Commit messages follow Conventional Commits format
- [ ] Documentation updated if needed
- [ ] REUSE compliance verified (`reuse lint`)
- [ ] Clear PR description explaining what and why
- [ ] Related issues referenced
- [ ] One logical change per PR

### What Must Be Separate PRs

The following must **never** be combined in a single PR:

- **Refactors** — Code restructuring without behavior change
- **Features** — New functionality
- **Third-party updates** — Library/dependency version bumps

Third-party library updates require standalone PRs with documented rationale
explaining why the update is needed.

---

## What Not to Do

1. **Don't mix refactors with features.** Each PR should contain one logical
   change.

2. **Don't skip sign-off.** The DCO bot will block your PR.

3. **Don't post raw AI output.** All contributions must reflect genuine
   understanding and personal competence.

4. **Don't submit without testing.** Run the test suite for affected
   sub-projects.

5. **Don't ignore CI failures.** Fix them before requesting review.

6. **Don't force-push to shared branches.** Only force-push to your own
   feature branches.

7. **Don't submit changes without REUSE compliance.** Every new file needs
   SPDX headers.

---

## Documentation

### Where Documentation Lives

| Location | Content |
|----------|---------|
| `docs/handbook/` | Developer handbook organized by sub-project |
| `docs/contributing/` | Contribution-specific guides |
| `docs/` | General documentation |
| `meshmc/doc/` | MeshMC-specific docs |
| `meshmc/BUILD.md` | MeshMC build instructions |
| `ofborg/doc/` | tickborg documentation |
| Sub-project `README.md` files | Per-component overviews |

### Documentation Standards

- Use Markdown for all documentation
- Follow the existing heading structure
- Include code examples where appropriate
- Cross-reference related documents
- Add SPDX license headers to new documentation files:
  ```
  <!-- SPDX-License-Identifier: CC0-1.0 -->
  ```

---

## Contact

| Channel | Address |
|---------|---------|
| GitHub Issues | [Project-Tick/Project-Tick/issues](https://github.com/Project-Tick/Project-Tick/issues) |
| Email | [projecttick@projecttick.org](mailto:projecttick@projecttick.org) |

---

## License

By contributing to Project Tick, you agree to license your work under the
project's applicable licenses. See the `LICENSES/` directory for details.

The specific license for each sub-project is tracked in `REUSE.toml`. Ensure
your contributions comply with the license of the sub-project you are
modifying.
