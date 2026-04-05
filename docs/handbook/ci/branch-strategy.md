# Branch Strategy

## Overview

The Project Tick monorepo uses a structured branch naming convention that enables
CI scripts to automatically classify branches, determine valid base branches for PRs,
and decide which checks to run. The classification logic lives in
`ci/supportedBranches.js`.

---

## Branch Naming Convention

### Format

```
prefix[-version[-suffix]]
```

Where:
- `prefix` — The branch type (e.g., `master`, `release`, `feature`)
- `version` — Optional semantic version (e.g., `1.0`, `2.5.1`)
- `suffix` — Optional additional descriptor (e.g., `pre`, `hotfix`)

### Parsing Regex

```javascript
/(?<prefix>[a-zA-Z-]+?)(-(?<version>\d+\.\d+(?:\.\d+)?)(?:-(?<suffix>.*))?)?$/
```

This regex extracts three named groups:

| Group     | Description                      | Example: `release-2.5.1-hotfix` |
|-----------|----------------------------------|---------------------------------|
| `prefix`  | Branch type identifier           | `release`                       |
| `version` | Semantic version number          | `2.5.1`                         |
| `suffix`  | Additional descriptor            | `hotfix`                        |

### Parse Examples

```javascript
split('master')
// { prefix: 'master', version: undefined, suffix: undefined }

split('release-1.0')
// { prefix: 'release', version: '1.0', suffix: undefined }

split('release-2.5.1')
// { prefix: 'release', version: '2.5.1', suffix: undefined }

split('staging-1.0')
// { prefix: 'staging', version: '1.0', suffix: undefined }

split('staging-next-1.0')
// { prefix: 'staging-next', version: '1.0', suffix: undefined }

split('feature-new-ui')
// { prefix: 'feature', version: undefined, suffix: undefined }
// Note: "new-ui" doesn't match version pattern, so prefix consumes it

split('fix-crash-on-start')
// { prefix: 'fix', version: undefined, suffix: undefined }

split('backport-123-to-release-1.0')
// { prefix: 'backport', version: undefined, suffix: undefined }
// Note: "123-to-release-1.0" doesn't start with a version, so no match

split('dependabot-npm')
// { prefix: 'dependabot', version: undefined, suffix: undefined }
```

---

## Branch Classification

### Type Configuration

```javascript
const typeConfig = {
  master: ['development', 'primary'],
  release: ['development', 'primary'],
  staging: ['development', 'secondary'],
  'staging-next': ['development', 'secondary'],
  feature: ['wip'],
  fix: ['wip'],
  backport: ['wip'],
  revert: ['wip'],
  wip: ['wip'],
  dependabot: ['wip'],
}
```

### Branch Types

| Prefix          | Type Tags                    | Description                         |
|----------------|------------------------------|-------------------------------------|
| `master`       | `development`, `primary`     | Main development branch             |
| `release`      | `development`, `primary`     | Release branches (e.g., `release-1.0`) |
| `staging`      | `development`, `secondary`   | Pre-release staging                 |
| `staging-next` | `development`, `secondary`   | Next staging cycle                  |
| `feature`      | `wip`                        | Feature development branches        |
| `fix`          | `wip`                        | Bug fix branches                    |
| `backport`     | `wip`                        | Backport branches                   |
| `revert`       | `wip`                        | Revert branches                     |
| `wip`          | `wip`                        | Work-in-progress branches           |
| `dependabot`   | `wip`                        | Automated dependency updates        |

Any branch with an unrecognized prefix defaults to type `['wip']`.

### Type Tag Meanings

| Tag           | Purpose                                                     |
|--------------|-------------------------------------------------------------|
| `development` | A long-lived branch that receives PRs                      |
| `primary`    | The main target for new work (master or release branches)   |
| `secondary`  | A staging area — receives from primary, not from WIP directly |
| `wip`        | A short-lived branch created for a specific task            |

---

## Order Configuration

Branch ordering determines which branch is preferred when multiple branches are
equally good candidates as PR base branches:

```javascript
const orderConfig = {
  master: 0,
  release: 1,
  staging: 2,
  'staging-next': 3,
}
```

| Branch Prefix   | Order | Preference                               |
|----------------|-------|------------------------------------------|
| `master`       | 0     | Highest — default target for new work    |
| `release`      | 1     | Second — for release-specific changes    |
| `staging`      | 2     | Third — staging area                     |
| `staging-next` | 3     | Fourth — next staging cycle              |
| All others     | `Infinity` | Lowest — not considered as base branches |

If two branches have the same number of commits ahead of a PR head, the one with
the lower order is preferred. This means `master` is preferred over `release-1.0`
when both are equally close.

---

## Classification Function

```javascript
function classify(branch) {
  const { prefix, version } = split(branch)
  return {
    branch,
    order: orderConfig[prefix] ?? Infinity,
    stable: version != null,
    type: typeConfig[prefix] ?? ['wip'],
    version: version ?? 'dev',
  }
}
```

### Output Fields

| Field     | Type     | Description                                          |
|----------|----------|------------------------------------------------------|
| `branch`  | String  | The original branch name                             |
| `order`   | Number  | Sort priority (lower = preferred as base)            |
| `stable`  | Boolean | `true` if the branch has a version suffix            |
| `type`    | Array   | Type tags from `typeConfig`                          |
| `version` | String  | Extracted version number, or `'dev'` if none         |

### Classification Examples

```javascript
classify('master')
// { branch: 'master', order: 0, stable: false, type: ['development', 'primary'], version: 'dev' }

classify('release-1.0')
// { branch: 'release-1.0', order: 1, stable: true, type: ['development', 'primary'], version: '1.0' }

classify('release-2.5.1')
// { branch: 'release-2.5.1', order: 1, stable: true, type: ['development', 'primary'], version: '2.5.1' }

classify('staging-1.0')
// { branch: 'staging-1.0', order: 2, stable: true, type: ['development', 'secondary'], version: '1.0' }

classify('staging-next-1.0')
// { branch: 'staging-next-1.0', order: 3, stable: true, type: ['development', 'secondary'], version: '1.0' }

classify('feature-new-ui')
// { branch: 'feature-new-ui', order: Infinity, stable: false, type: ['wip'], version: 'dev' }

classify('fix-crash-on-start')
// { branch: 'fix-crash-on-start', order: Infinity, stable: false, type: ['wip'], version: 'dev' }

classify('dependabot-npm')
// { branch: 'dependabot-npm', order: Infinity, stable: false, type: ['wip'], version: 'dev' }

classify('wip-experiment')
// { branch: 'wip-experiment', order: Infinity, stable: false, type: ['wip'], version: 'dev' }

classify('random-unknown-branch')
// { branch: 'random-unknown-branch', order: Infinity, stable: false, type: ['wip'], version: 'dev' }
```

---

## Branch Flow Model

### Development Flow

```
┌─────────────────────────────────────────────┐
│                  master                      │
│  (primary development, receives all work)    │
└──────────┬──────────────────────┬───────────┘
           │ fork                  │ fork
           ▼                      ▼
┌──────────────────┐   ┌──────────────────────┐
│  staging-X.Y     │   │  release-X.Y         │
│  (secondary,     │   │  (primary,           │
│   pre-release)   │   │   stable release)    │
└──────────────────┘   └──────────────────────┘
```

### WIP Branch Flow

```
    master (or release-X.Y)
         │
    ┌────┴────┐
    │ fork    │
    ▼         │
  feature-*   │
  fix-*       │
  backport-*  │
  wip-*       │
    │         │
    └──── PR ─┘
    (merged back)
```

### Typical Branch Lifecycle

1. **Create** — Developer creates `feature-my-change` from `master`
2. **Develop** — Commits follow Conventional Commits format
3. **PR** — Pull request targets `master` (or the appropriate release branch)
4. **CI Validation** — `prepare.js` classifies branches, `lint-commits.js` checks messages
5. **Review** — Code review by owners defined in `ci/OWNERS`
6. **Merge** — PR is merged into the target branch
7. **Cleanup** — The WIP branch is deleted

---

## How CI Uses Branch Classification

### Commit Linting Exemptions

PRs between development branches skip commit linting:

```javascript
if (
  baseBranchType.includes('development') &&
  headBranchType.includes('development') &&
  pr.base.repo.id === pr.head.repo?.id
) {
  core.info('This PR is from one development branch to another. Skipping checks.')
  return
}
```

Exempted transitions:
- `staging` → `master`
- `staging-next` → `staging`
- `release-X.Y` → `master`

### Base Branch Suggestion

For WIP branches, `prepare.js` finds the optimal base:

1. Start with `master` as a candidate
2. Compare commit distances to all `release-*` branches (sorted newest first)
3. The branch with fewest commits ahead is the best candidate
4. On ties, lower `order` wins (master > release > staging)

### Release Branch Targeting Warning

When a non-backport/fix/revert branch targets a release branch:

```
Warning: This PR targets release branch `release-1.0`.
New features should typically target `master`.
```

---

## Version Extraction

The `stable` flag and `version` field enable version-aware CI decisions:

| Branch             | `stable` | `version` | Interpretation                  |
|-------------------|----------|-----------|--------------------------------|
| `master`          | `false`  | `'dev'`   | Development, no specific version |
| `release-1.0`    | `true`   | `'1.0'`   | Release 1.0                     |
| `release-2.5.1`  | `true`   | `'2.5.1'` | Release 2.5.1                   |
| `staging-1.0`    | `true`   | `'1.0'`   | Staging for release 1.0         |
| `feature-foo`    | `false`  | `'dev'`   | WIP, no version association      |

Release branches are sorted by version (descending) when computing base branch
suggestions, so `release-2.0` is checked before `release-1.0`.

---

## Module Exports

The `supportedBranches.js` module exports two functions:

```javascript
module.exports = { classify, split }
```

| Function   | Purpose                                                  |
|-----------|----------------------------------------------------------|
| `classify` | Full classification: type tags, order, stability, version|
| `split`    | Parse branch name into prefix, version, suffix           |

These are imported by:
- `ci/github-script/lint-commits.js` — For commit linting exemptions
- `ci/github-script/prepare.js` — For branch targeting validation

---

## Self-Testing

When `supportedBranches.js` is run directly (not imported as a module), it executes
built-in tests:

```bash
cd ci/
node supportedBranches.js
```

Output:

```
split(branch)
master { prefix: 'master', version: undefined, suffix: undefined }
release-1.0 { prefix: 'release', version: '1.0', suffix: undefined }
release-2.5.1 { prefix: 'release', version: '2.5.1', suffix: undefined }
staging-1.0 { prefix: 'staging', version: '1.0', suffix: undefined }
staging-next-1.0 { prefix: 'staging-next', version: '1.0', suffix: undefined }
feature-new-ui { prefix: 'feature', version: undefined, suffix: undefined }
fix-crash-on-start { prefix: 'fix', version: undefined, suffix: undefined }
...

classify(branch)
master { branch: 'master', order: 0, stable: false, type: ['development', 'primary'], version: 'dev' }
release-1.0 { branch: 'release-1.0', order: 1, stable: true, type: ['development', 'primary'], version: '1.0' }
...
```

---

## Adding New Branch Types

To add a new branch type:

1. Add the prefix and type tags to `typeConfig`:

```javascript
const typeConfig = {
  // ... existing entries ...
  'hotfix': ['wip'],  // or ['development', 'primary'] if it's a long-lived branch
}
```

2. If it should be a base branch candidate, add it to `orderConfig`:

```javascript
const orderConfig = {
  // ... existing entries ...
  hotfix: 4,  // lower number = higher preference
}
```

3. Update the self-tests at the bottom of the file.
