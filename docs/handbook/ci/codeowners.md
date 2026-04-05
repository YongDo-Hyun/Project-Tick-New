# CODEOWNERS

## Overview

Project Tick uses a code ownership system based on the `ci/OWNERS` file. This file
follows the same syntax as GitHub's native `CODEOWNERS` file but is stored in a
custom location and validated by a patched version of the
[codeowners-validator](https://github.com/mszostok/codeowners-validator) tool.

The OWNERS file serves two purposes:
1. **Automated review routing** — PR authors know who to request reviews from
2. **Structural validation** — CI checks that referenced paths and users exist

---

## File Location and Format

### Location

```
ci/OWNERS
```

Unlike GitHub's native CODEOWNERS (which must be in `.github/CODEOWNERS`,
`CODEOWNERS`, or `docs/CODEOWNERS`), Project Tick stores ownership data in
`ci/OWNERS` to keep CI infrastructure colocated.

### Syntax

The file uses CODEOWNERS syntax:

```
# Comments start with #
# Pattern followed by one or more @owner references
/path/pattern/    @owner1 @owner2
```

### Header

```
# This file describes who owns what in the Project Tick CI infrastructure.
# Users/teams will get review requests for PRs that change their files.
#
# This file uses the same syntax as the natively supported CODEOWNERS file,
# see https://help.github.com/articles/about-codeowners/ for documentation.
#
# Validated by ci/codeowners-validator.
```

---

## Ownership Map

The OWNERS file maps every major directory and subdirectory in the monorepo to
code owners. Below is the complete ownership mapping:

### GitHub Infrastructure

```
/.github/actions/change-analysis/                    @YongDo-Hyun
/.github/actions/meshmc/package/                     @YongDo-Hyun
/.github/actions/meshmc/setup-dependencies/          @YongDo-Hyun
/.github/actions/mnv/test_artefacts/                 @YongDo-Hyun
/.github/codeql/                                     @YongDo-Hyun
/.github/ISSUE_TEMPLATE/                             @YongDo-Hyun
/.github/workflows/                                  @YongDo-Hyun
```

### Archived Projects

```
/archived/projt-launcher/                            @YongDo-Hyun
/archived/projt-minicraft-modpack/                   @YongDo-Hyun
/archived/projt-modpack/                             @YongDo-Hyun
/archived/ptlibzippy/                                @YongDo-Hyun
```

### Core Projects

```
/cgit/*                                              @YongDo-Hyun
/cgit/contrib/*                                      @YongDo-Hyun
/cgit/contrib/hooks/                                 @YongDo-Hyun
/cgit/filters/                                       @YongDo-Hyun
/cgit/tests/                                         @YongDo-Hyun

/cmark/*                                             @YongDo-Hyun
/cmark/api_test/                                     @YongDo-Hyun
/cmark/bench/                                        @YongDo-Hyun
/cmark/cmake/                                        @YongDo-Hyun
/cmark/data/                                         @YongDo-Hyun
/cmark/fuzz/                                         @YongDo-Hyun
/cmark/man/                                          @YongDo-Hyun
/cmark/src/                                          @YongDo-Hyun
/cmark/test/                                         @YongDo-Hyun
/cmark/tools/                                        @YongDo-Hyun
/cmark/wrappers/                                     @YongDo-Hyun
```

### Corebinutils (every utility individually owned)

```
/corebinutils/*                                      @YongDo-Hyun
/corebinutils/cat/                                   @YongDo-Hyun
/corebinutils/chflags/                               @YongDo-Hyun
/corebinutils/chmod/                                 @YongDo-Hyun
/corebinutils/contrib/*                              @YongDo-Hyun
/corebinutils/contrib/libc-vis/                      @YongDo-Hyun
/corebinutils/contrib/libedit/                       @YongDo-Hyun
/corebinutils/contrib/printf/                        @YongDo-Hyun
/corebinutils/cp/                                    @YongDo-Hyun
...
/corebinutils/uuidgen/                               @YongDo-Hyun
```

### Other Projects

```
/forgewrapper/*                                      @YongDo-Hyun
/forgewrapper/gradle/                                @YongDo-Hyun
/forgewrapper/jigsaw/                                @YongDo-Hyun
/forgewrapper/src/                                   @YongDo-Hyun

/genqrcode/*                                         @YongDo-Hyun
/genqrcode/cmake/                                    @YongDo-Hyun
/genqrcode/tests/                                    @YongDo-Hyun
/genqrcode/use/                                      @YongDo-Hyun

/hooks/                                              @YongDo-Hyun
/images4docker/                                      @YongDo-Hyun

/json4cpp/*                                          @YongDo-Hyun
/json4cpp/.reuse/                                    @YongDo-Hyun
/json4cpp/cmake/                                     @YongDo-Hyun
/json4cpp/docs/                                      @YongDo-Hyun
/json4cpp/include/*                                  @YongDo-Hyun
...

/libnbtplusplus/*                                    @YongDo-Hyun
/libnbtplusplus/include/*                            @YongDo-Hyun
...

/LICENSES/                                           @YongDo-Hyun

/meshmc/*                                            @YongDo-Hyun
/meshmc/branding/                                    @YongDo-Hyun
/meshmc/buildconfig/                                 @YongDo-Hyun
/meshmc/cmake/*                                      @YongDo-Hyun
/meshmc/launcher/*                                   @YongDo-Hyun
...
```

---

## Pattern Syntax

### Glob Rules

| Pattern        | Matches                                              |
|---------------|------------------------------------------------------|
| `/path/`      | All files directly under `path/`                     |
| `/path/*`     | All files directly under `path/` (explicit)          |
| `/path/**`    | All files recursively under `path/`                  |
| `*.js`        | All `.js` files everywhere                           |
| `/path/*.md`  | All `.md` files directly under `path/`               |

### Ownership Resolution

When multiple patterns match a file, the **last matching rule** wins (just like
Git's `.gitignore` and GitHub's native CODEOWNERS):

```
/meshmc/*                  @teamA    # Matches all direct files
/meshmc/launcher/*         @teamB    # More specific — wins for launcher files
```

A PR modifying `meshmc/launcher/main.cpp` would require review from `@teamB`.

### Explicit Directory Listing

The OWNERS file explicitly lists individual subdirectories rather than using `**`
recursive globs. This is intentional:

1. **Precision** — Each directory has explicit ownership
2. **Validation** — The codeowners-validator checks that each listed path exists
3. **Documentation** — The file serves as a directory map of the monorepo

---

## Validation

### codeowners-validator

The CI runs a patched version of `codeowners-validator` against the OWNERS file.
The tool is built from source with Project Tick–specific patches.

#### What It Validates

| Check                    | Description                                    |
|-------------------------|------------------------------------------------|
| **Path existence**       | All paths in OWNERS exist in the repository    |
| **User/team existence**  | All `@` references are valid GitHub users/teams|
| **Syntax**               | Pattern syntax is valid CODEOWNERS format      |
| **No orphaned patterns** | Patterns match at least one file               |

#### Custom Patches

Two patches are applied to the upstream validator:

**1. Custom OWNERS file path** (`owners-file-name.patch`)

```go
func openCodeownersFile(dir string) (io.Reader, error) {
    if file, ok := os.LookupEnv("OWNERS_FILE"); ok {
        return fs.Open(file)
    }
    // ... default CODEOWNERS paths
}
```

Set `OWNERS_FILE=ci/OWNERS` to validate the custom location.

**2. Removed write-access requirement** (`permissions.patch`)

GitHub's native CODEOWNERS requires that listed users have write access to the
repository. Project Tick's OWNERS file is used for review routing, not branch
protection, so this check is removed:

```go
// Before: required push permission
if t.Permissions["push"] { return nil }
return newValidateError("Team cannot review PRs...")

// After: any team membership is sufficient
return nil
```

Also removes the `github.ScopeReadOrg` requirement from required OAuth scopes,
allowing the validator to work with tokens generated for GitHub Apps.

### Running Validation Locally

```bash
cd ci/
nix-shell  # enters the CI dev shell with codeowners-validator available

# Set the custom OWNERS file path:
export OWNERS_FILE=ci/OWNERS

# Run validation:
codeowners-validator
```

Or build and run directly:

```bash
nix-build ci/ -A codeownersValidator
OWNERS_FILE=ci/OWNERS ./result/bin/codeowners-validator
```

---

## MAINTAINERS File Relationship

In addition to `ci/OWNERS`, individual projects may have a `MAINTAINERS` file
(e.g., `archived/projt-launcher/MAINTAINERS`):

```
# MAINTAINERS
#
# Fields:
# - Name: Display name
# - GitHub: GitHub handle (with @)
# - Email: Primary contact email
# - Paths: Comma-separated glob patterns (repo-relative)

[Mehmet Samet Duman]
GitHub: @YongDo-Hyun
Email: yongdohyun@mail.projecttick.org
Paths: **
```

The `MAINTAINERS` file provides additional metadata (email, display name) that
`OWNERS` doesn't support. The two files serve complementary purposes:

| File          | Purpose                              | Format            |
|--------------|--------------------------------------|-------------------|
| `ci/OWNERS`  | Automated review routing via CI      | CODEOWNERS syntax |
| `MAINTAINERS`| Human-readable contact information   | INI-style blocks  |

---

## Review Requirements

### How Reviews Are Triggered

When a PR modifies files matching an OWNERS pattern:

1. The workflow identifies which owners are responsible for the changed paths
2. Review requests are sent to the matching owners
3. At least one approving review from a code owner is typically required before merge

### Bot-Managed Reviews

The CI bot (`github-actions[bot]`) manages automated reviews via `ci/github-script/reviews.js`:
- Reviews are tagged with a `reviewKey` comment for identification
- When issues are resolved, bot reviews are automatically dismissed or minimized
- The `CHANGES_REQUESTED` state blocks merge until the review is dismissed

---

## Adding Ownership Entries

### For a New Project Directory

1. Add ownership patterns to `ci/OWNERS`:

```
/newproject/*                                        @owner-handle
/newproject/src/                                     @owner-handle
/newproject/tests/                                   @owner-handle
```

2. List every subdirectory explicitly (not just the top-level with `**`)

3. Run the validator locally:

```bash
cd ci/
nix-shell
OWNERS_FILE=ci/OWNERS codeowners-validator
```

4. Commit with a CI scope:

```
ci(repo): add ownership for newproject
```

### For a New Team or User

Simply reference the new `@handle` in the ownership patterns:

```
/some/path/    @existing-owner @new-owner
```

The validator will check that `@new-owner` exists in the GitHub organization.

---

## Limitations

### No Recursive Globs in Current File

The current OWNERS file uses explicit directory listings rather than `/**` recursive
globs. This means:
- New subdirectories must be manually added to OWNERS
- Deeply nested directories need their own entries
- The file can grow large for projects with many subdirectories

### Single Organization Scope

All `@` references must be members of the repository's GitHub organization,
or GitHub users with access to the repository.

### No Per-File Patterns

The file doesn't currently use file-level patterns (e.g., `*.nix @nix-team`).
Ownership is assigned at the directory level.
