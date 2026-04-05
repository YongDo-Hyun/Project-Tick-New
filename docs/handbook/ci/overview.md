# CI Infrastructure — Overview

## Purpose

The `ci/` directory contains the Continuous Integration infrastructure for the Project Tick monorepo.
It provides reproducible builds, automated code quality checks, commit message validation,
pull request lifecycle management, and code ownership enforcement — all orchestrated through
Nix expressions and JavaScript-based GitHub Actions scripts.

The CI system is designed around three core principles:

1. **Reproducibility** — Pinned Nix dependencies ensure identical builds across environments
2. **Conventional Commits** — Enforced commit message format for automated changelog generation
3. **Ownership-driven review** — CODEOWNERS-style file ownership with automated review routing

---

## Directory Structure

```
ci/
├── OWNERS                           # Code ownership file (CODEOWNERS format)
├── README.md                        # CI README with local testing instructions
├── default.nix                      # Nix CI entry point — treefmt, codeowners-validator, shell
├── pinned.json                      # Pinned Nixpkgs + treefmt-nix revisions (npins format)
├── update-pinned.sh                 # Script to update pinned.json via npins
├── supportedBranches.js             # Branch classification logic for CI decisions
├── codeowners-validator/            # Builds codeowners-validator from source (Go)
│   ├── default.nix                  # Nix derivation for codeowners-validator
│   ├── owners-file-name.patch       # Patch: custom OWNERS file path via OWNERS_FILE env var
│   └── permissions.patch            # Patch: remove write-access check (not needed for non-native CODEOWNERS)
└── github-script/                   # JavaScript CI scripts for GitHub Actions
    ├── run                          # CLI entry point for local testing (commander-based)
    ├── lint-commits.js              # Commit message linter (Conventional Commits)
    ├── prepare.js                   # PR preparation: mergeability, branch targeting, touched files
    ├── reviews.js                   # Review lifecycle: post, dismiss, minimize bot reviews
    ├── get-pr-commit-details.js     # Extract commit SHAs, subjects, changed paths via git
    ├── withRateLimit.js             # GitHub API rate limiting with Bottleneck
    ├── package.json                 # Node.js dependencies (@actions/core, @actions/github, bottleneck, commander)
    ├── package-lock.json            # Lockfile for reproducible npm installs
    ├── shell.nix                    # Nix dev shell for github-script (Node.js + gh CLI)
    ├── README.md                    # Local testing documentation
    ├── .editorconfig                # Editor configuration
    ├── .gitignore                   # Git ignore rules
    └── .npmrc                       # npm configuration
```

---

## How CI Works End-to-End

### 1. Triggering

CI runs are triggered by GitHub Actions workflows (defined in `.github/workflows/`) when
pull requests are opened, updated, or merged against supported branches. The `supportedBranches.js`
module classifies branches to determine which checks to run.

### 2. Environment Setup

The CI environment is bootstrapped via `ci/default.nix`, which:

- Reads pinned dependency revisions from `ci/pinned.json`
- Fetches the exact Nixpkgs tarball at the pinned commit
- Imports `treefmt-nix` for code formatting
- Builds the `codeowners-validator` tool with Project Tick–specific patches
- Exposes a development shell with all CI tools available

```nix
# ci/default.nix — entry point
let
  pinned = (builtins.fromJSON (builtins.readFile ./pinned.json)).pins;
in
{
  system ? builtins.currentSystem,
  nixpkgs ? null,
}:
let
  nixpkgs' =
    if nixpkgs == null then
      fetchTarball {
        inherit (pinned.nixpkgs) url;
        sha256 = pinned.nixpkgs.hash;
      }
    else
      nixpkgs;

  pkgs = import nixpkgs' {
    inherit system;
    config = { };
    overlays = [ ];
  };
```

### 3. Code Formatting (treefmt)

The `default.nix` configures `treefmt-nix` with multiple formatters:

| Formatter    | Purpose                              | Configuration                                |
|-------------|--------------------------------------|----------------------------------------------|
| `actionlint` | GitHub Actions workflow linting      | Enabled, no custom config                    |
| `biome`      | JavaScript/TypeScript formatting     | Single quotes, no semicolons, no JSON format |
| `keep-sorted`| Sorted list enforcement              | Enabled, no custom config                    |
| `nixfmt`     | Nix expression formatting            | Uses `pkgs.nixfmt`                           |
| `yamlfmt`    | YAML formatting                      | Retains line breaks                          |
| `zizmor`     | GitHub Actions security scanning     | Enabled, no custom config                    |

Biome is configured with specific style rules:

```nix
programs.biome = {
  enable = true;
  validate.enable = false;
  settings.formatter = {
    useEditorconfig = true;
  };
  settings.javascript.formatter = {
    quoteStyle = "single";
    semicolons = "asNeeded";
  };
  settings.json.formatter.enabled = false;
};
settings.formatter.biome.excludes = [
  "*.min.js"
];
```

### 4. Commit Linting

When a PR is opened or updated, `ci/github-script/lint-commits.js` validates every commit
message against the Conventional Commits specification. It checks:

- Format: `type(scope): subject`
- No `fixup!`, `squash!`, or `amend!` prefixes (must be rebased before merge)
- No trailing period on subject line
- Lowercase first letter in subject
- Known scopes matching monorepo project directories

The supported types are:

```javascript
const CONVENTIONAL_TYPES = [
  'build', 'chore', 'ci', 'docs', 'feat', 'fix',
  'perf', 'refactor', 'revert', 'style', 'test',
]
```

And the known scopes correspond to monorepo directories:

```javascript
const KNOWN_SCOPES = [
  'archived', 'cgit', 'ci', 'cmark', 'corebinutils',
  'forgewrapper', 'genqrcode', 'hooks', 'images4docker',
  'json4cpp', 'libnbtplusplus', 'meshmc', 'meta', 'mnv',
  'neozip', 'tomlplusplus', 'repo', 'deps',
]
```

### 5. PR Preparation and Validation

The `ci/github-script/prepare.js` script handles PR lifecycle:

1. **Mergeability check** — Polls GitHub's mergeability computation with exponential backoff
   (5s, 10s, 20s, 40s, 80s retries)
2. **Branch classification** — Classifies base and head branches using `supportedBranches.js`
3. **Base branch suggestion** — For WIP branches, computes the optimal base branch by comparing
   merge-base commit distances across `master` and all release branches
4. **Merge conflict detection** — If the PR has conflicts, uses the head SHA directly; otherwise
   uses the merge commit SHA
5. **Touched file detection** — Identifies which CI-relevant paths were modified:
   - `ci` — any file under `ci/`
   - `pinned` — `ci/pinned.json` specifically
   - `github` — any file under `.github/`

### 6. Review Lifecycle Management

The `ci/github-script/reviews.js` module manages bot reviews:

- **`postReview()`** — Posts or updates a review with a tracking comment tag
  (`<!-- projt review key: <key>; resolved: false -->`)
- **`dismissReviews()`** — Dismisses, minimizes (marks as outdated), or resolves bot reviews
  when the underlying issue is fixed
- Reviews are tagged with a `reviewKey` to allow multiple independent review concerns
  on the same PR

### 7. Rate Limiting

All GitHub API calls go through `ci/github-script/withRateLimit.js`, which uses the
Bottleneck library for request throttling:

- Read requests: controlled by a reservoir updated from the GitHub rate limit API
- Write requests (`POST`, `PUT`, `PATCH`, `DELETE`): minimum 1 second between calls
- The reservoir keeps 1000 spare requests for other concurrent jobs
- Reservoir is refreshed every 60 seconds
- Requests to `github.com` (not the API), `/rate_limit`, and `/search/` endpoints bypass throttling

### 8. Code Ownership Validation

The `ci/codeowners-validator/` builds a patched version of the
[codeowners-validator](https://github.com/mszostok/codeowners-validator) tool:

- Fetched from GitHub at a specific pinned commit
- Two patches applied:
  - `owners-file-name.patch` — Adds support for custom CODEOWNERS file path via `OWNERS_FILE` env var
  - `permissions.patch` — Removes the write-access permission check (not needed since Project Tick
    uses an `OWNERS` file rather than GitHub's native `CODEOWNERS`)

This validates the `ci/OWNERS` file against the actual repository structure and GitHub
organization membership.

---

## Component Interaction Flow

```
┌─────────────────────────────────────────┐
│           GitHub Actions Workflow        │
│         (.github/workflows/*.yml)        │
└──────────────┬──────────────────────────┘
               │ triggers
               ▼
┌──────────────────────────────────────────┐
│          ci/default.nix                   │
│  ┌─────────┐  ┌──────────────────────┐   │
│  │pinned.  │  │ treefmt-nix          │   │
│  │json     │──│ (formatting checks)  │   │
│  └─────────┘  └──────────────────────┘   │
│               ┌──────────────────────┐   │
│               │ codeowners-validator │   │
│               │ (OWNERS validation)  │   │
│               └──────────────────────┘   │
└──────────────┬───────────────────────────┘
               │ also triggers
               ▼
┌──────────────────────────────────────────┐
│       ci/github-script/                   │
│  ┌────────────────┐  ┌───────────────┐   │
│  │ prepare.js      │  │ lint-commits │   │
│  │ (PR validation) │  │ (commit msg) │   │
│  └───────┬────────┘  └──────┬────────┘   │
│          │                   │            │
│  ┌───────▼────────┐  ┌──────▼────────┐   │
│  │ reviews.js      │  │ supported    │   │
│  │ (bot reviews)   │  │ Branches.js  │   │
│  └───────┬────────┘  └───────────────┘   │
│          │                                │
│  ┌───────▼────────┐                      │
│  │ withRateLimit   │                      │
│  │ (API throttle)  │                      │
│  └────────────────┘                      │
└──────────────────────────────────────────┘
```

---

## Key Design Decisions

### Why Nix for CI?

Nix ensures that every CI run uses the exact same versions of tools, compilers, and
libraries. The `pinned.json` file locks specific commits of Nixpkgs and treefmt-nix,
eliminating "works on my machine" problems.

### Why a custom OWNERS file?

GitHub's native CODEOWNERS has limitations:
- Must be in `.github/CODEOWNERS`, `CODEOWNERS`, or `docs/CODEOWNERS`
- Requires repository write access for all listed owners
- Cannot be extended with custom validation

Project Tick uses `ci/OWNERS` with the same glob pattern syntax but adds:
- Custom file path support (via the `OWNERS_FILE` environment variable)
- No write-access requirement (via the permissions patch)
- Integration with the codeowners-validator for structural validation

### Why Bottleneck for rate limiting?

GitHub Actions can run many jobs in parallel, and each job makes API calls. Without
throttling, a large CI run could exhaust the GitHub API rate limit (5000 requests/hour
for authenticated requests). Bottleneck provides:
- Concurrency control (1 concurrent request by default)
- Reservoir-based rate limiting (dynamically updated from the API)
- Separate throttling for mutative requests (1 second minimum between writes)

### Why local testing support?

The `ci/github-script/run` CLI allows developers to test CI scripts locally before
pushing. This accelerates development and reduces CI feedback loops:

```bash
cd ci/github-script
nix-shell     # sets up Node.js + dependencies
gh auth login # authenticate with GitHub
./run lint-commits YongDo-Hyun Project-Tick 123
./run prepare YongDo-Hyun Project-Tick 123
```

---

## Pinned Dependencies

The CI system pins two external Nix sources:

| Dependency   | Source                                       | Branch             | Purpose                        |
|-------------|----------------------------------------------|--------------------|--------------------------------|
| `nixpkgs`   | `github:NixOS/nixpkgs`                      | `nixpkgs-unstable` | Base package set for CI tools  |
| `treefmt-nix`| `github:numtide/treefmt-nix`               | `main`             | Multi-formatter orchestrator   |

Pins are stored in `ci/pinned.json` in npins v5 format:

```json
{
  "pins": {
    "nixpkgs": {
      "type": "Git",
      "repository": {
        "type": "GitHub",
        "owner": "NixOS",
        "repo": "nixpkgs"
      },
      "branch": "nixpkgs-unstable",
      "revision": "bde09022887110deb780067364a0818e89258968",
      "url": "https://github.com/NixOS/nixpkgs/archive/bde09022887110deb780067364a0818e89258968.tar.gz",
      "hash": "13mi187zpa4rw680qbwp7pmykjia8cra3nwvjqmsjba3qhlzif5l"
    },
    "treefmt-nix": {
      "type": "Git",
      "repository": {
        "type": "GitHub",
        "owner": "numtide",
        "repo": "treefmt-nix"
      },
      "branch": "main",
      "revision": "e96d59dff5c0d7fddb9d113ba108f03c3ef99eca",
      "url": "https://github.com/numtide/treefmt-nix/archive/e96d59dff5c0d7fddb9d113ba108f03c3ef99eca.tar.gz",
      "hash": "02gqyxila3ghw8gifq3mns639x86jcq079kvfvjm42mibx7z5fzb"
    }
  },
  "version": 5
}
```

To update pins:

```bash
cd ci/
./update-pinned.sh
```

This runs `npins --lock-file pinned.json update` to fetch the latest revisions.

---

## Node.js Dependencies (github-script)

The `ci/github-script/package.json` declares:

```json
{
  "private": true,
  "dependencies": {
    "@actions/core": "1.11.1",
    "@actions/github": "6.0.1",
    "bottleneck": "2.19.5",
    "commander": "14.0.3"
  }
}
```

| Package           | Version  | Purpose                                       |
|-------------------|----------|-----------------------------------------------|
| `@actions/core`   | `1.11.1` | GitHub Actions core utilities (logging, outputs) |
| `@actions/github` | `6.0.1`  | GitHub API client (Octokit wrapper)           |
| `bottleneck`      | `2.19.5` | Rate limiting / request throttling            |
| `commander`       | `14.0.3` | CLI argument parsing for local `./run` tool   |

These versions are kept in sync with the
[actions/github-script](https://github.com/actions/github-script) action.

---

## Nix Dev Shell

The `ci/github-script/shell.nix` provides a development environment for working on
the CI scripts locally:

```nix
{
  system ? builtins.currentSystem,
  pkgs ? (import ../../ci { inherit system; }).pkgs,
}:

pkgs.callPackage (
  {
    gh,
    importNpmLock,
    mkShell,
    nodejs,
  }:
  mkShell {
    packages = [
      gh
      importNpmLock.hooks.linkNodeModulesHook
      nodejs
    ];

    npmDeps = importNpmLock.buildNodeModules {
      npmRoot = ./.;
      inherit nodejs;
    };
  }
) { }
```

This gives you:
- `nodejs` — Node.js runtime
- `gh` — GitHub CLI for authentication
- `importNpmLock.hooks.linkNodeModulesHook` — Automatically links `node_modules` from the Nix store

---

## Outputs Exposed by default.nix

The `ci/default.nix` exposes the following attributes:

| Attribute             | Type      | Description                                      |
|----------------------|-----------|--------------------------------------------------|
| `pkgs`               | Nixpkgs   | The pinned Nixpkgs package set                   |
| `fmt.shell`          | Derivation| Dev shell with treefmt formatter available        |
| `fmt.pkg`            | Derivation| The treefmt wrapper binary                        |
| `fmt.check`          | Derivation| A check derivation that fails if formatting drifts|
| `codeownersValidator`| Derivation| Patched codeowners-validator binary               |
| `shell`              | Derivation| Combined CI dev shell (fmt + codeowners-validator)|

```nix
rec {
  inherit pkgs fmt;
  codeownersValidator = pkgs.callPackage ./codeowners-validator { };

  shell = pkgs.mkShell {
    packages = [
      fmt.pkg
      codeownersValidator
    ];
  };
}
```

---

## Integration with Root Flake

The root `flake.nix` provides:

- Dev shells for all supported systems (`aarch64-linux`, `x86_64-linux`, etc.)
- A formatter (`nixfmt-rfc-style`)
- The CI `default.nix` is imported indirectly via the flake for Nix-based CI runs

```nix
{
  description = "Project Tick is a project dedicated to providing developers
    with ease of use and users with long-lasting software.";

  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixos-unstable/nixexprs.tar.xz";
  };
  ...
}
```

---

## Summary of CI Checks

| Check                    | Tool / Script             | Scope                              |
|--------------------------|---------------------------|------------------------------------|
| Code formatting          | treefmt (biome, nixfmt, yamlfmt, actionlint, zizmor) | All source files |
| Commit message format    | `lint-commits.js`         | All commits in a PR               |
| PR mergeability          | `prepare.js`              | Every PR                          |
| Base branch targeting    | `prepare.js` + `supportedBranches.js` | WIP → development PRs   |
| Code ownership validity  | `codeowners-validator`    | `ci/OWNERS` file                  |
| GitHub Actions security  | `zizmor` (via treefmt)    | `.github/workflows/*.yml`         |
| Sorted list enforcement  | `keep-sorted` (via treefmt)| Files with keep-sorted markers   |

---

## Related Documentation

- [Nix Infrastructure](nix-infrastructure.md) — Deep dive into the Nix expressions
- [Commit Linting](commit-linting.md) — Commit message conventions and validation rules
- [PR Validation](pr-validation.md) — Pull request checks and lifecycle management
- [Branch Strategy](branch-strategy.md) — Branch naming, classification, and release branches
- [CODEOWNERS](codeowners.md) — Ownership file format and validation
- [Formatting](formatting.md) — Code formatting configuration and tools
- [Rate Limiting](rate-limiting.md) — GitHub API rate limiting strategy
