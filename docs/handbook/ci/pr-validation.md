# PR Validation

## Overview

The `ci/github-script/prepare.js` script runs on every pull request to validate
mergeability, classify branches, suggest optimal base branches, detect merge conflicts,
and identify which CI-relevant paths were touched. It also manages bot review comments
to guide contributors toward correct PR targeting.

---

## What prepare.js Does

1. **Checks PR state** — Ensures the PR is still open
2. **Waits for mergeability** — Polls GitHub until mergeability is computed
3. **Classifies branches** — Categorizes base and head branches using `supportedBranches.js`
4. **Validates branch targeting** — Warns if a feature branch targets a release branch
5. **Suggests better base branches** — For WIP branches, finds the optimal base by comparing
   commit distances
6. **Computes merge SHAs** — Determines the merge commit SHA and target comparison SHA
7. **Detects touched CI paths** — Identifies changes to `ci/`, `ci/pinned.json`, `.github/`

---

## Mergeability Check

GitHub computes merge status asynchronously. The script polls with exponential backoff:

```javascript
for (const retryInterval of [5, 10, 20, 40, 80]) {
  core.info('Checking whether the pull request can be merged...')
  const prInfo = (
    await github.rest.pulls.get({
      ...context.repo,
      pull_number,
    })
  ).data

  if (prInfo.state !== 'open') throw new Error('PR is not open anymore.')

  if (prInfo.mergeable == null) {
    core.info(
      `GitHub is still computing mergeability, waiting ${retryInterval}s...`,
    )
    await new Promise((resolve) => setTimeout(resolve, retryInterval * 1000))
    continue
  }
  // ... process PR
}
throw new Error(
  'Timed out waiting for GitHub to compute mergeability. Check https://www.githubstatus.com.',
)
```

### Retry Schedule

| Attempt | Wait Time | Cumulative Wait |
|---------|-----------|-----------------|
| 1       | 5 seconds | 5 seconds       |
| 2       | 10 seconds| 15 seconds      |
| 3       | 20 seconds| 35 seconds      |
| 4       | 40 seconds| 75 seconds      |
| 5       | 80 seconds| 155 seconds     |

If mergeability is still not computed after ~2.5 minutes, the script throws an error
with a link to [githubstatus.com](https://www.githubstatus.com) for checking GitHub's
system status.

---

## Branch Classification

Both the base and head branches are classified using `supportedBranches.js`:

```javascript
const baseClassification = classify(base.ref)
core.setOutput('base', baseClassification)

const headClassification =
  base.repo.full_name === head.repo.full_name
    ? classify(head.ref)
    : { type: ['wip'] }
core.setOutput('head', headClassification)
```

### Fork Handling

For cross-fork PRs (where the head repo differs from the base repo), the head branch
is always classified as `{ type: ['wip'] }` regardless of its name. This prevents
fork branches from being treated as development branches.

### Classification Output

Each classification produces:

```javascript
{
  branch: 'release-1.0',
  order: 1,
  stable: true,
  type: ['development', 'primary'],
  version: '1.0',
}
```

| Field      | Description                                          |
|-----------|------------------------------------------------------|
| `branch`   | The full branch name                                |
| `order`    | Ranking for base-branch preference (lower = better) |
| `stable`   | Whether the branch has a version suffix             |
| `type`     | Array of type tags                                  |
| `version`  | Extracted version number, or `'dev'`                |

---

## Release Branch Targeting Warning

If a WIP branch (feature, fix, etc.) targets a stable release branch, the script
checks whether it's a backport:

```javascript
if (
  baseClassification.stable &&
  baseClassification.type.includes('primary')
) {
  const headPrefix = head.ref.split('-')[0]
  if (!['backport', 'fix', 'revert'].includes(headPrefix)) {
    core.warning(
      `This PR targets release branch \`${base.ref}\`. ` +
        'New features should typically target \`master\`.',
    )
  }
}
```

| Head Branch Prefix | Allowed to target release? | Reason              |
|-------------------|---------------------------|---------------------|
| `backport-*`      | Yes                       | Explicit backport   |
| `fix-*`           | Yes                       | Bug fix for release |
| `revert-*`        | Yes                       | Reverting a change  |
| `feature-*`       | Warning issued            | Should target master|
| `wip-*`           | Warning issued            | Should target master|

---

## Base Branch Suggestion

For WIP branches, the script computes the optimal base branch by analyzing commit
distances from the head to all candidate base branches:

### Algorithm

1. **List all branches** — Fetch all branches in the repository via pagination
2. **Filter candidates** — Keep `master` and all stable primary branches (release-*)
3. **Compute merge bases** — For each candidate, find the merge-base commit with the
   PR head and count commits between them

```javascript
async function mergeBase({ branch, order, version }) {
  const { data } = await github.rest.repos.compareCommitsWithBasehead({
    ...context.repo,
    basehead: `${branch}...${head.sha}`,
    per_page: 1,
    page: 2,
  })
  return {
    branch,
    order,
    version,
    commits: data.total_commits,
    sha: data.merge_base_commit.sha,
  }
}
```

4. **Select the best** — The branch with the fewest commits ahead wins. If there's a tie,
   the branch with the lowest `order` wins (i.e., `master` over `release-*`).

```javascript
let candidates = [await mergeBase(classify('master'))]
for (const release of releases) {
  const nextCandidate = await mergeBase(release)
  if (candidates[0].commits === nextCandidate.commits)
    candidates.push(nextCandidate)
  if (candidates[0].commits > nextCandidate.commits)
    candidates = [nextCandidate]
  if (candidates[0].commits < 10000) break
}

const best = candidates.sort((a, b) => a.order - b.order).at(0)
```

5. **Post review if mismatch** — If the suggested base differs from the current base,
   a bot review is posted:

```javascript
if (best.branch !== base.ref) {
  const current = await mergeBase(classify(base.ref))
  const body = [
    `This PR targets \`${current.branch}\`, but based on the commit history ` +
      `\`${best.branch}\` appears to be a better fit ` +
      `(${current.commits - best.commits} fewer commits ahead).`,
    '',
    `If this is intentional, you can ignore this message. Otherwise:`,
    `- [Change the base branch](...) to \`${best.branch}\`.`,
  ].join('\n')

  await postReview({ github, context, core, dry, body, reviewKey })
}
```

6. **Dismiss reviews if correct** — If the base branch matches the suggestion, any
   previous bot reviews are dismissed.

### Early Termination

The algorithm stops evaluating release branches once the candidate count drops below
10,000 commits. This prevents unnecessary API calls for branches that are clearly
not good candidates.

---

## Merge SHA Computation

The script computes two key SHAs for downstream CI jobs:

### Mergeable PR

```javascript
if (prInfo.mergeable) {
  core.info('The PR can be merged.')
  mergedSha = prInfo.merge_commit_sha
  targetSha = (
    await github.rest.repos.getCommit({
      ...context.repo,
      ref: prInfo.merge_commit_sha,
    })
  ).data.parents[0].sha
}
```

- `mergedSha` — GitHub's trial merge commit SHA
- `targetSha` — The first parent of the merge commit (base branch tip)

### Conflicting PR

```javascript
else {
  core.warning('The PR has a merge conflict.')
  mergedSha = head.sha
  targetSha = (
    await github.rest.repos.compareCommitsWithBasehead({
      ...context.repo,
      basehead: `${base.sha}...${head.sha}`,
    })
  ).data.merge_base_commit.sha
}
```

- `mergedSha` — Falls back to the head SHA (no merge commit exists)
- `targetSha` — The merge-base between base and head

---

## Touched Path Detection

The script identifies which CI-relevant paths were modified in the PR:

```javascript
const files = (
  await github.paginate(github.rest.pulls.listFiles, {
    ...context.repo,
    pull_number,
    per_page: 100,
  })
).map((file) => file.filename)

const touched = []
if (files.some((f) => f.startsWith('ci/'))) touched.push('ci')
if (files.includes('ci/pinned.json')) touched.push('pinned')
if (files.some((f) => f.startsWith('.github/'))) touched.push('github')
core.setOutput('touched', touched)
```

| Touched Tag | Condition                                | Use Case                        |
|------------|------------------------------------------|---------------------------------|
| `ci`       | Any file under `ci/` was changed         | Re-run CI infrastructure checks |
| `pinned`   | `ci/pinned.json` specifically changed    | Validate pin integrity          |
| `github`   | Any file under `.github/` was changed    | Re-run workflow lint checks     |

---

## Outputs

The script sets the following outputs for downstream workflow jobs:

| Output       | Type   | Description                                       |
|-------------|--------|---------------------------------------------------|
| `base`       | Object | Base branch classification (branch, type, version) |
| `head`       | Object | Head branch classification                        |
| `mergedSha`  | String | Merge commit SHA (or head SHA if conflicting)     |
| `targetSha`  | String | Base comparison SHA                               |
| `touched`    | Array  | Which CI-relevant paths were modified             |

---

## Review Lifecycle

The `prepare.js` script integrates with `reviews.js` for bot review management:

### Posting a Review

When the script detects a branch targeting issue, it posts a `REQUEST_CHANGES` review:

```javascript
await postReview({ github, context, core, dry, body, reviewKey: 'prepare' })
```

The review body includes:
- A description of the issue
- A comparison of commit distances
- A link to GitHub's "change base branch" documentation

### Dismissing Reviews

When the issue is resolved (correct base branch), previous reviews are dismissed:

```javascript
await dismissReviews({ github, context, core, dry, reviewKey: 'prepare' })
```

The `reviewKey` (`'prepare'`) ensures only reviews posted by this script are affected.

---

## Dry Run Mode

When the `--no-dry` flag is NOT passed (default in local testing), all mutative
operations (posting/dismissing reviews) are skipped:

```javascript
module.exports = async ({ github, context, core, dry }) => {
  // ...
  if (!dry) {
    await github.rest.pulls.createReview({ ... })
  }
}
```

This allows safe local testing without modifying real PRs.

---

## Local Testing

```bash
cd ci/github-script
nix-shell
gh auth login

# Dry run (default — no changes to the PR):
./run prepare YongDo-Hyun Project-Tick 123

# Live run (actually posts/dismisses reviews):
./run prepare YongDo-Hyun Project-Tick 123 --no-dry
```

---

## Error Conditions

| Condition                            | Behavior                                     |
|-------------------------------------|----------------------------------------------|
| PR is closed                         | Throws: `"PR is not open anymore."`         |
| Mergeability timeout                 | Throws: `"Timed out waiting for GitHub..."` |
| API rate limit exceeded              | Handled by `withRateLimit.js`               |
| Merge conflict                       | Warning issued; head SHA used as mergedSha  |
| Wrong base branch                    | REQUEST_CHANGES review posted               |
