# Contributing

## Overview

This document summarizes the contribution guidelines for MeshMC. For the full authoritative guide, see [CONTRIBUTING.md](../../../meshmc/CONTRIBUTING.md) in the MeshMC source tree.

## AI Policy

MeshMC follows a strict AI usage policy adapted from matplotlib and the Linux Kernel:

- **No raw AI output** as comments on GitHub or Discord
- If AI tools are used to develop code or documentation, the contributor **must fully understand** the changes and explain why they are the correct approach
- Contributions must demonstrate personal competency and added value
- Low-quality AI-generated contributions will be rigorously rejected

### AI Agent Restrictions

- AI agents **MUST NOT** add `Signed-off-by` tags — only humans can certify the Developer Certificate of Origin
- The human submitter is responsible for reviewing all AI-generated code, ensuring licensing compliance, and taking full responsibility

### AI Attribution

When AI tools contribute to development, include an `Assisted-by` tag in the commit message:

```
Assisted-by: AGENT_NAME:MODEL_VERSION [TOOL1] [TOOL2]
```

Example:
```
Assisted-by: Claude:claude-3-opus coccinelle sparse
```

Basic development tools (git, gcc, make, editors) do not need to be listed.

## Bootstrapping

Before building, run the bootstrap script:

```bash
# Linux / macOS
./bootstrap.sh

# Windows
.\bootstrap.cmd
```

This installs:
- lefthook (git hooks)
- reuse (license compliance)
- Go tools
- zlib, extra-cmake-modules
- Other build dependencies

**Note**: Qt6 with modules must be installed separately. The bootstrap script installs Qt only for QuaZip's needs. On Windows, use the Qt Online Installer.

## Building

See [Building MeshMC](building.md) for complete build instructions.

## Signing Your Work (DCO)

All contributions must be signed off using the Developer Certificate of Origin (DCO).

### How to Sign Off

Append `-s` to your git commit:
```bash
git commit -s -m "Fix instance loading crash"
```

Or manually append:
```
Fix instance loading crash

Signed-off-by: Your Name <your.email@example.com>
```

### Developer Certificate of Origin 1.1

By signing off, you certify:

1. **(a)** The contribution was created by you and you have the right to submit it under the project's open source license
2. **(b)** The contribution is based on existing work covered under an appropriate open source license
3. **(c)** The contribution was provided to you by someone who certified (a), (b), or (c)
4. **(d)** You understand this is a public record maintained indefinitely

### Enforcement

Sign-off is enforced automatically when creating a pull request. You will be notified if any commits aren't signed off.

### Cryptographic Signing (Optional)

You can also [cryptographically sign commits](https://docs.github.com/en/authentication/managing-commit-signature-verification/signing-commits) and enable [vigilant mode](https://docs.github.com/en/authentication/managing-commit-signature-verification/displaying-verification-statuses-for-all-of-your-commits) on GitHub.

## Contributor License Agreement (CLA)

By submitting a contribution, you agree to the [Project Tick CLA](https://projecttick.org/licenses/PT-CLA-2.0.txt).

The CLA ensures:
- You have the legal right to submit the contribution
- It does not knowingly infringe third-party rights
- Project Tick may distribute the contribution under the applicable license
- Long-term governance and license consistency are maintained

The CLA applies to all intentional contributions: source code, documentation, tests, data, media, and configuration files.

## Backporting

Automated backports merge specific contributions from `develop` into `release` branches:

- Add labels like `backport release-7.x` to PRs
- Add the milestone for the target release
- The [backport workflow](https://github.com/Project-Tick/MeshMC/blob/master/.github/workflows/backport.yml) handles the merge automatically

## Pull Request Workflow

1. Fork the repository
2. Create a feature branch from `develop`
3. Make your changes
4. Ensure code passes `clang-format` and `clang-tidy` checks
5. Sign off all commits (`git commit -s`)
6. Include AI attribution if applicable
7. Open a PR against `develop`
8. Address review feedback
9. Add backport labels if the fix applies to release branches

## License

MeshMC is licensed under GPL-3.0-or-later. All contributions must be compatible with this license. The project uses REUSE for license compliance tracking.
