# Commit Linting

## Overview

Project Tick enforces the [Conventional Commits](https://www.conventionalcommits.org/)
specification for all commit messages. The commit linter (`ci/github-script/lint-commits.js`)
runs automatically on every pull request to validate that every commit follows the required
format.

This ensures:
- Consistent, machine-readable commit history
- Automated changelog generation potential
- Clear communication of change intent (feature, fix, refactor, etc.)
- Monorepo-aware scoping that maps commits to project directories

---

## Commit Message Format

### Structure

```
type(scope): subject
```

### Examples

```
feat(mnv): add new keybinding support
fix(meshmc): resolve crash on startup
ci(neozip): update build matrix
docs(cmark): fix API reference
refactor(corebinutils): simplify ls output logic
chore(deps): bump tomlplusplus to v4.0.0
revert(forgewrapper): undo jigsaw module changes
```

### Rules

| Rule                          | Requirement                                              |
|-------------------------------|----------------------------------------------------------|
| **Type**                      | Must be one of the supported types (see below)           |
| **Scope**                     | Optional, but should match a known project directory     |
| **Subject**                   | Must follow the type/scope with `: ` (colon + space)     |
| **Trailing period**           | Subject must NOT end with a period                       |
| **Subject case**              | Subject should start with a lowercase letter (warning)   |
| **No fixup/squash commits**  | `fixup!`, `squash!`, `amend!` prefixes are rejected      |
| **Breaking changes**          | Use `!` after type/scope: `feat(mnv)!: remove API`      |

---

## Supported Types

The following Conventional Commit types are recognized:

```javascript
const CONVENTIONAL_TYPES = [
  'build',
  'chore',
  'ci',
  'docs',
  'feat',
  'fix',
  'perf',
  'refactor',
  'revert',
  'style',
  'test',
]
```

| Type       | Use When                                                   |
|-----------|-------------------------------------------------------------|
| `build`   | Changes to the build system or external dependencies        |
| `chore`   | Routine tasks, no production code change                    |
| `ci`      | CI configuration files and scripts                          |
| `docs`    | Documentation only changes                                  |
| `feat`    | A new feature                                               |
| `fix`     | A bug fix                                                   |
| `perf`    | A performance improvement                                   |
| `refactor`| Code change that neither fixes a bug nor adds a feature     |
| `revert`  | Reverts a previous commit                                   |
| `style`   | Formatting, semicolons, whitespace (no code change)         |
| `test`    | Adding or correcting tests                                  |

---

## Known Scopes

Scopes correspond to directories in the Project Tick monorepo:

```javascript
const KNOWN_SCOPES = [
  'archived',
  'cgit',
  'ci',
  'cmark',
  'corebinutils',
  'forgewrapper',
  'genqrcode',
  'hooks',
  'images4docker',
  'json4cpp',
  'libnbtplusplus',
  'meshmc',
  'meta',
  'mnv',
  'neozip',
  'tomlplusplus',
  'repo',
  'deps',
]
```

### Special Scopes

| Scope    | Meaning                                            |
|----------|----------------------------------------------------|
| `repo`   | Changes affecting the repository as a whole        |
| `deps`   | Dependency updates not scoped to a single project  |

### Unknown Scope Handling

Using an unknown scope generates a **warning** (not an error):

```
Commit abc123456789: scope "myproject" is not a known project.
Known scopes: archived, cgit, ci, cmark, ...
```

This allows new scopes to be introduced before updating the linter.

---

## Validation Logic

### Regex Pattern

The commit message is validated against this regex:

```javascript
const conventionalRegex = new RegExp(
  `^(${CONVENTIONAL_TYPES.join('|')})(\\(([^)]+)\\))?(!)?: .+$`,
)
```

Expanded, this matches:

```
^(build|chore|ci|docs|feat|fix|perf|refactor|revert|style|test)  # type
(\(([^)]+)\))?                                                     # optional (scope)
(!)?                                                               # optional breaking change marker
: .+$                                                              # colon, space, and subject
```

### Validation Order

For each commit in the PR:

1. **Check for fixup/squash/amend** — If the message starts with `amend!`, `fixup!`, or
   `squash!`, the commit fails immediately. These commits should be rebased before merging:

   ```javascript
   const fixups = ['amend!', 'fixup!', 'squash!']
   if (fixups.some((s) => msg.startsWith(s))) {
     core.error(
       `${logPrefix}: starts with "${fixups.find((s) => msg.startsWith(s))}". ` +
         'Did you forget to run `git rebase -i --autosquash`?',
     )
     failures.add(commit.sha)
     continue
   }
   ```

2. **Check Conventional Commits format** — If the regex doesn't match, the commit fails:

   ```javascript
   if (!conventionalRegex.test(msg)) {
     core.error(
       `${logPrefix}: "${msg}" does not follow Conventional Commits format. ` +
         'Expected: type(scope): subject  (e.g. "feat(mnv): add keybinding")',
     )
     failures.add(commit.sha)
     continue
   }
   ```

3. **Check trailing period** — Subjects ending with `.` fail:

   ```javascript
   if (msg.endsWith('.')) {
     core.error(`${logPrefix}: subject should not end with a period.`)
     failures.add(commit.sha)
   }
   ```

4. **Warn on unknown scope** — Non-standard scopes produce a warning:

   ```javascript
   if (scope && !KNOWN_SCOPES.includes(scope)) {
     core.warning(
       `${logPrefix}: scope "${scope}" is not a known project. ` +
         `Known scopes: ${KNOWN_SCOPES.join(', ')}`,
     )
     warnings.add(commit.sha)
   }
   ```

5. **Warn on uppercase subject** — If the first character after `: ` is uppercase, warn:

   ```javascript
   const subjectStart = msg.indexOf(': ') + 2
   if (subjectStart < msg.length) {
     const firstChar = msg[subjectStart]
     if (firstChar === firstChar.toUpperCase() && firstChar !== firstChar.toLowerCase()) {
       core.warning(`${logPrefix}: subject should start with lowercase letter.`)
       warnings.add(commit.sha)
     }
   }
   ```

---

## Branch-Based Exemptions

The linter skips validation for PRs between development branches:

```javascript
const baseBranchType = classify(pr.base.ref.replace(/^refs\/heads\//, '')).type
const headBranchType = classify(pr.head.ref.replace(/^refs\/heads\//, '')).type

if (
  baseBranchType.includes('development') &&
  headBranchType.includes('development') &&
  pr.base.repo.id === pr.head.repo?.id
) {
  core.info('This PR is from one development branch to another. Skipping checks.')
  return
}
```

This exempts:
- `staging` → `master` merges
- `staging-next` → `staging` merges
- `release-X.Y` → `master` merges

These are infrastructure merges where commits were already validated in their original PRs.

The `classify()` function from `supportedBranches.js` determines branch types:

| Branch Prefix   | Type                    | Exempt as PR source? |
|----------------|-------------------------|---------------------|
| `master`       | `development`, `primary` | Yes                 |
| `release-*`    | `development`, `primary` | Yes                 |
| `staging-*`    | `development`, `secondary` | Yes              |
| `staging-next-*`| `development`, `secondary` | Yes             |
| `feature-*`    | `wip`                   | No                  |
| `fix-*`        | `wip`                   | No                  |
| `backport-*`   | `wip`                   | No                  |

---

## Commit Detail Extraction

The linter uses `get-pr-commit-details.js` to extract commit information. Notably,
this uses **git directly** rather than the GitHub API:

```javascript
async function getCommitDetailsForPR({ core, pr, repoPath }) {
  await runGit({
    args: ['fetch', `--depth=1`, 'origin', pr.base.sha],
    repoPath, core,
  })
  await runGit({
    args: ['fetch', `--depth=${pr.commits + 1}`, 'origin', pr.head.sha],
    repoPath, core,
  })

  const shas = (
    await runGit({
      args: [
        'rev-list',
        `--max-count=${pr.commits}`,
        `${pr.base.sha}..${pr.head.sha}`,
      ],
      repoPath, core,
    })
  ).stdout.split('\n').map((s) => s.trim()).filter(Boolean)
```

### Why Not Use the GitHub API?

The GitHub REST API's "list commits on a PR" endpoint has a hard limit of **250 commits**.
For large PRs or release-branch merges, this is insufficient. Using git directly:
- Has no commit count limit
- Also returns changed file paths per commit (used for scope validation)
- Is faster for bulk operations

For each commit, the script extracts:

| Field                 | Source                      | Purpose                          |
|----------------------|-----------------------------|---------------------------------|
| `sha`                 | `git rev-list`             | Commit identifier                |
| `subject`             | `git log --format=%s`      | First line of commit message     |
| `changedPaths`        | `git log --name-only`      | Files changed in that commit     |
| `changedPathSegments` | Path splitting             | Directory segments for scope matching |

---

## Error Output

### Failures (block merge)

```
Error: Commit abc123456789: "Add new feature" does not follow Conventional Commits format.
Expected: type(scope): subject  (e.g. "feat(mnv): add keybinding")

Error: Commit def456789012: starts with "fixup!".
Did you forget to run `git rebase -i --autosquash`?

Error: Commit ghi789012345: subject should not end with a period.

Error: Please review the Conventional Commits guidelines at
<https://www.conventionalcommits.org/> and the project CONTRIBUTING.md.

Error: 3 commit(s) do not follow commit conventions.
```

### Warnings (informational)

```
Warning: Commit jkl012345678: scope "myproject" is not a known project.
Known scopes: archived, cgit, ci, cmark, ...

Warning: Commit mno345678901: subject should start with lowercase letter.

Warning: 2 commit(s) have minor issues (see warnings above).
```

---

## Local Testing

Test the commit linter locally using the CLI runner:

```bash
cd ci/github-script
nix-shell                                        # enter Nix dev shell
gh auth login                                    # authenticate with GitHub
./run lint-commits YongDo-Hyun Project-Tick 123   # lint PR #123
```

The `./run` CLI uses the `commander` package and authenticates via `gh auth token`:

```javascript
program
  .command('lint-commits')
  .description('Lint commit messages for Conventional Commits compliance.')
  .argument('<owner>', 'Repository owner (e.g. YongDo-Hyun)')
  .argument('<repo>', 'Repository name (e.g. Project-Tick)')
  .argument('<pr>', 'Pull Request number')
  .action(async (owner, repo, pr) => {
    const lint = (await import('./lint-commits.js')).default
    await run(lint, owner, repo, pr)
  })
```

---

## Best Practices

### Writing Good Commit Messages

1. **Use the correct type** — `feat` for features, `fix` for bugs, `docs` for documentation
2. **Include a scope** — Helps identify which project is affected: `feat(meshmc): ...`
3. **Use imperative mood** — "add feature" not "added feature" or "adds feature"
4. **Keep subject under 72 characters** — For readability in `git log`
5. **Start with lowercase** — `add feature` not `Add feature`
6. **No trailing period** — `fix(cgit): resolve parse error` not `fix(cgit): resolve parse error.`

### Handling Fixup Commits During Development

During development, you can use `git commit --fixup=<sha>` freely. Before opening
the PR (or before requesting review), squash them:

```bash
git rebase -i --autosquash origin/master
```

### Multiple Scopes

If a commit touches multiple projects, either:
- Use `repo` as the scope: `refactor(repo): update shared build config`
- Use the primary affected project as the scope
- Split the commit into separate per-project commits

---

## Adding New Types or Scopes

### New Scope

Add the scope to the `KNOWN_SCOPES` array in `ci/github-script/lint-commits.js`:

```javascript
const KNOWN_SCOPES = [
  'archived',
  'cgit',
  // ...
  'newproject',  // ← add here (keep sorted)
  // ...
]
```

### New Type

Adding new types requires updating `CONVENTIONAL_TYPES` — but this should be done
rarely, as the standard Conventional Commits types cover most use cases.
