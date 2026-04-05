# Nix Infrastructure

## Overview

The CI system for the Project Tick monorepo is built on Nix, using pinned dependency
sources to guarantee reproducible builds and formatting checks. The primary entry point
is `ci/default.nix`, which bootstraps the complete CI toolchain from `ci/pinned.json`.

This document covers the Nix expressions in detail: how they work, what they produce,
and how they integrate with the broader Project Tick build infrastructure.

---

## ci/default.nix — The CI Entry Point

The `default.nix` file is the sole entry point for all Nix-based CI operations. It:

1. Reads pinned source revisions from `pinned.json`
2. Fetches the exact Nixpkgs tarball
3. Configures the treefmt multi-formatter
4. Builds the codeowners-validator
5. Exposes a development shell with all CI tools

### Top-level Structure

```nix
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

### Function Parameters

| Parameter  | Default                      | Purpose                                         |
|-----------|------------------------------|-------------------------------------------------|
| `system`   | `builtins.currentSystem`    | Target system (e.g., `x86_64-linux`)            |
| `nixpkgs`  | `null` (uses pinned)        | Override Nixpkgs source for development/testing |

When `nixpkgs` is `null` (the default), the pinned revision is fetched. When provided
explicitly, the override is used instead — useful for testing against newer Nixpkgs.

### Importing Nixpkgs

The Nixpkgs tarball is imported with empty config and no overlays:

```nix
pkgs = import nixpkgs' {
  inherit system;
  config = { };
  overlays = [ ];
};
```

This ensures a "pure" package set with no user-specific customizations that could
break CI reproducibility.

---

## Pinned Dependencies (pinned.json)

### Format

The `pinned.json` file uses the [npins](https://github.com/andir/npins) v5 format. It
stores Git-based pins with full provenance information:

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
      "submodules": false,
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
      "submodules": false,
      "revision": "e96d59dff5c0d7fddb9d113ba108f03c3ef99eca",
      "url": "https://github.com/numtide/treefmt-nix/archive/e96d59dff5c0d7fddb9d113ba108f03c3ef99eca.tar.gz",
      "hash": "02gqyxila3ghw8gifq3mns639x86jcq079kvfvjm42mibx7z5fzb"
    }
  },
  "version": 5
}
```

### Pin Fields

| Field         | Description                                                |
|--------------|------------------------------------------------------------|
| `type`        | Source type (`Git`)                                        |
| `repository`  | Source location (`GitHub` with owner + repo)               |
| `branch`      | Upstream branch being tracked                              |
| `submodules`   | Whether to fetch Git submodules (`false`)                 |
| `revision`    | Full commit SHA of the pinned revision                     |
| `url`         | Direct tarball download URL for the pinned revision        |
| `hash`        | SRI hash (base32) for integrity verification               |

### Why Two Pins?

| Pin            | Tracked Branch       | Purpose                                    |
|---------------|----------------------|--------------------------------------------|
| `nixpkgs`     | `nixpkgs-unstable`   | Base package set: compilers, tools, libraries |
| `treefmt-nix` | `main`               | Code formatter orchestrator and its modules |

The `nixpkgs-unstable` branch is used rather than a release branch to get recent
tool versions while still being reasonably stable.

---

## Updating Pinned Dependencies

### update-pinned.sh

The update script is minimal:

```bash
#!/usr/bin/env nix-shell
#!nix-shell -i bash -p npins

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

npins --lock-file pinned.json update
```

This:

1. Enters a `nix-shell` with `npins` available
2. Changes to the `ci/` directory (where `pinned.json` lives)
3. Runs `npins update` to fetch the latest commit from each tracked branch
4. Updates `pinned.json` with new revisions and hashes

### When to Update

- **Regularly**: To pick up security patches and tool updates
- **When a formatter change is needed**: New treefmt-nix releases may add formatters
- **When CI breaks on upstream**: Pin to a known-good revision

### Manual Update Procedure

```bash
# From the repository root:
cd ci/
./update-pinned.sh

# Review the diff:
git diff pinned.json

# Test locally:
nix-build -A fmt.check

# Commit:
git add pinned.json
git commit -m "ci: update pinned nixpkgs and treefmt-nix"
```

---

## treefmt Integration

### What is treefmt?

[treefmt](https://github.com/numtide/treefmt) is a multi-language formatter orchestrator.
It runs multiple formatters in parallel and ensures every file type has exactly one formatter.
The `treefmt-nix` module provides a Nix-native way to configure it.

### Configuration in default.nix

```nix
fmt =
  let
    treefmtNixSrc = fetchTarball {
      inherit (pinned.treefmt-nix) url;
      sha256 = pinned.treefmt-nix.hash;
    };
    treefmtEval = (import treefmtNixSrc).evalModule pkgs {
      projectRootFile = ".git/config";

      settings.verbose = 1;
      settings.on-unmatched = "debug";

      programs.actionlint.enable = true;

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

      programs.keep-sorted.enable = true;

      programs.nixfmt = {
        enable = true;
        package = pkgs.nixfmt;
      };

      programs.yamlfmt = {
        enable = true;
        settings.formatter = {
          retain_line_breaks = true;
        };
      };

      programs.zizmor.enable = true;
    };
```

### treefmt Settings

| Setting                     | Value         | Purpose                                     |
|----------------------------|---------------|---------------------------------------------|
| `projectRootFile`          | `.git/config` | Marker file to detect the repository root   |
| `settings.verbose`        | `1`           | Show which formatter processes each file    |
| `settings.on-unmatched`   | `"debug"`     | Log unmatched files at debug level          |

### Configured Formatters

#### actionlint
- **Purpose**: Lint GitHub Actions workflow YAML files
- **Scope**: `.github/workflows/*.yml`
- **Configuration**: Default settings

#### biome
- **Purpose**: Format JavaScript and TypeScript files
- **Configuration**:
  - `useEditorconfig = true` — Respects `.editorconfig` settings
  - `quoteStyle = "single"` — Uses single quotes
  - `semicolons = "asNeeded"` — Only adds semicolons where required by ASI
  - `validate.enable = false` — No lint-level validation, only formatting
  - `json.formatter.enabled = false` — Does not format JSON files
- **Exclusions**: `*.min.js` — Minified JavaScript files are skipped

#### keep-sorted
- **Purpose**: Enforces sorted order in marked sections (e.g., dependency lists)
- **Configuration**: Default settings

#### nixfmt
- **Purpose**: Format Nix expressions
- **Package**: Uses `pkgs.nixfmt` from the pinned Nixpkgs
- **Configuration**: Default nixfmt-rfc-style formatting

#### yamlfmt
- **Purpose**: Format YAML files
- **Configuration**:
  - `retain_line_breaks = true` — Preserves intentional blank lines

#### zizmor
- **Purpose**: Security scanning for GitHub Actions workflows
- **Configuration**: Default settings
- **Detects**: Injection vulnerabilities, insecure defaults, untrusted inputs

### Formatter Source Tree

The treefmt evaluation creates a source tree from the repository, excluding `.git`:

```nix
fs = pkgs.lib.fileset;
src = fs.toSource {
  root = ../.;
  fileset = fs.difference ../. (fs.maybeMissing ../.git);
};
```

This ensures the formatting check operates on the full repository contents while
avoiding Git internals.

### Outputs

The `fmt` attribute set exposes three derivations:

```nix
{
  shell = treefmtEval.config.build.devShell;   # nix develop .#fmt.shell
  pkg = treefmtEval.config.build.wrapper;      # treefmt binary
  check = treefmtEval.config.build.check src;  # nix build .#fmt.check
}
```

| Output      | Type        | Purpose                                          |
|------------|-------------|--------------------------------------------------|
| `fmt.shell` | Dev shell  | Interactive shell with treefmt available          |
| `fmt.pkg`   | Binary     | The treefmt wrapper with all formatters configured|
| `fmt.check` | Check      | A Nix derivation that fails if any file needs reformatting |

---

## codeowners-validator Derivation

### Purpose

The codeowners-validator checks that the `ci/OWNERS` file is structurally valid:
- All referenced paths exist in the repository
- All referenced GitHub users/teams exist in the organization
- Glob patterns are syntactically correct

### Build Definition

```nix
{
  buildGoModule,
  fetchFromGitHub,
  fetchpatch,
}:
buildGoModule {
  name = "codeowners-validator";
  src = fetchFromGitHub {
    owner = "mszostok";
    repo = "codeowners-validator";
    rev = "f3651e3810802a37bd965e6a9a7210728179d076";
    hash = "sha256-5aSmmRTsOuPcVLWfDF6EBz+6+/Qpbj66udAmi1CLmWQ=";
  };
  patches = [
    (fetchpatch {
      name = "user-write-access-check";
      url = "https://github.com/mszostok/codeowners-validator/compare/f3651e3...840eeb8.patch";
      hash = "sha256-t3Dtt8SP9nbO3gBrM0nRE7+G6N/ZIaczDyVHYAG/6mU=";
    })
    ./permissions.patch
    ./owners-file-name.patch
  ];
  postPatch = "rm -r docs/investigation";
  vendorHash = "sha256-R+pW3xcfpkTRqfS2ETVOwG8PZr0iH5ewroiF7u8hcYI=";
}
```

### Patches Applied

#### 1. user-write-access-check (upstream PR #222)
Fetched from the upstream repository. Modifies the write-access validation logic.

#### 2. permissions.patch
Undoes part of the upstream PR's write-access requirement:

```diff
 var reqScopes = map[github.Scope]struct{}{
-	github.ScopeReadOrg: {},
 }
```

And removes the push permission checks for teams and users:

```diff
 for _, t := range v.repoTeams {
     if strings.EqualFold(t.GetSlug(), team) {
-        if t.Permissions["push"] {
-            return nil
-        }
-        return newValidateError(...)
+        return nil
     }
 }
```

This is necessary because Project Tick's OWNERS file is used for code review routing,
not for GitHub's native branch protection rules. Contributors listed in OWNERS don't
need write access to the repository.

#### 3. owners-file-name.patch
Adds support for a custom CODEOWNERS file path via the `OWNERS_FILE` environment variable:

```diff
 func openCodeownersFile(dir string) (io.Reader, error) {
+	if file, ok := os.LookupEnv("OWNERS_FILE"); ok {
+		return fs.Open(file)
+	}
+
 	var detectedFiles []string
```

This allows the validator to check `ci/OWNERS` instead of the default `.github/CODEOWNERS`
or `CODEOWNERS` paths.

---

## CI Dev Shell

The top-level `shell` attribute combines all CI tools:

```nix
shell = pkgs.mkShell {
  packages = [
    fmt.pkg
    codeownersValidator
  ];
};
```

This provides:
- `treefmt` — The configured multi-formatter
- `codeowners-validator` — The patched OWNERS validator

Enter the shell:

```bash
cd ci/
nix-shell     # or: nix develop
treefmt       # format all files
codeowners-validator  # validate OWNERS
```

---

## github-script Nix Shell

The `ci/github-script/shell.nix` provides a separate dev shell for JavaScript CI scripts:

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

### Key Features

1. **Shared Nixpkgs**: Imports the pinned `pkgs` from `../../ci` (the parent `default.nix`)
2. **Node.js**: Full Node.js runtime for running CI scripts
3. **GitHub CLI**: `gh` for authentication (`gh auth token` is used by the `run` CLI)
4. **npm Lockfile Integration**: `importNpmLock` builds `node_modules` from `package-lock.json`
   in the Nix store, then `linkNodeModulesHook` symlinks it into the working directory

---

## Relationship to Root flake.nix

The root `flake.nix` defines the overall development environment:

```nix
{
  description = "Project Tick is a project dedicated to providing developers
    with ease of use and users with long-lasting software.";

  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixos-unstable/nixexprs.tar.xz";
  };

  outputs = { self, nixpkgs }:
    let
      systems = lib.systems.flakeExposed;
      forAllSystems = lib.genAttrs systems;
      nixpkgsFor = forAllSystems (system: nixpkgs.legacyPackages.${system});
    in
    {
      devShells = forAllSystems (system: ...);
      formatter = forAllSystems (system: nixpkgsFor.${system}.nixfmt-rfc-style);
    };
}
```

The flake's `inputs.nixpkgs` uses `nixos-unstable` via Nix channels, while the CI
`pinned.json` uses a specific commit from `nixpkgs-unstable`. These are related but
independently pinned — the flake updates when `flake.lock` is refreshed, while CI
pins update only when `update-pinned.sh` is explicitly run.

### When Each Is Used

| Context            | Nix Source                                    |
|-------------------|-----------------------------------------------|
| `nix develop`      | Root `flake.nix` → `flake.lock` → nixpkgs   |
| CI formatting check| `ci/default.nix` → `ci/pinned.json` → nixpkgs|
| CI script dev shell| `ci/github-script/shell.nix` → `ci/default.nix` |

---

## Evaluation and Build Commands

### Building the Format Check

```bash
# From repository root:
nix-build ci/ -A fmt.check

# Or with flakes:
nix build .#fmt.check
```

This produces a derivation that:
1. Copies the entire source tree (minus `.git`) into the Nix store
2. Runs all configured formatters
3. Fails with a diff if any file would be reformatted

### Entering the CI Shell

```bash
# Nix classic:
nix-shell ci/

# Nix flakes:
nix develop ci/
```

### Building codeowners-validator

```bash
nix-build ci/ -A codeownersValidator
./result/bin/codeowners-validator
```

---

## Troubleshooting

### "hash mismatch" on pinned.json update

If `update-pinned.sh` produces a hash mismatch, the upstream source has changed
at the same branch tip. Re-run the update:

```bash
cd ci/
./update-pinned.sh
```

### Formatter version mismatch

If local formatting produces different results than CI:

1. Ensure you're using the same Nixpkgs pin: `nix-shell ci/`
2. Run `treefmt` from within the CI shell
3. If the issue persists, update pins: `./update-pinned.sh`

### codeowners-validator fails to build

The Go module build requires network access for vendored dependencies. Ensure:
- The `vendorHash` in `codeowners-validator/default.nix` matches the actual Go module checksum
- If upstream dependencies change, update `vendorHash`

---

## Security Considerations

- **Hash verification**: All fetched tarballs are verified against their SRI hashes
- **No overlays**: Nixpkgs is imported with empty overlays to prevent supply-chain attacks
- **Pinned revisions**: Exact commit SHAs prevent upstream branch tampering
- **zizmor**: GitHub Actions workflows are scanned for injection vulnerabilities
- **actionlint**: Workflow syntax is validated to catch misconfigurations

---

## Summary

The Nix infrastructure provides:

1. **Reproducibility** — Identical tools and versions across all CI runs and developer machines
2. **Composability** — Each component (treefmt, codeowners-validator) is independently buildable
3. **Security** — Hash-verified dependencies, security scanning, no arbitrary overlays
4. **Developer experience** — `nix-shell` provides a ready-to-use environment with zero manual setup
