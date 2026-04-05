# Project Tick — Security Policy

## Overview

Project Tick takes the security of its software ecosystem seriously. This
document describes how to report security vulnerabilities, the disclosure
process, and the security practices applied across the monorepo.

Given that Project Tick includes components ranging from compression libraries
(NeoZip) to a full application (MeshMC) to CI infrastructure (tickborg), a
vulnerability in any sub-project could have cascading effects. The project
maintains a unified security posture across all components.

---

## Reporting Vulnerabilities

### How to Report

If you discover a security vulnerability in any Project Tick component, report
it via email:

**[projecttick@projecttick.org](mailto:projecttick@projecttick.org)**

### Do NOT

- Open a public GitHub issue for security vulnerabilities
- Post vulnerability details on Discord or social media
- Publish exploit code before the issue is resolved

### What to Include

When submitting a security report, include as much of the following as
possible:

| Field | Description |
|-------|-------------|
| **Affected component** | Which sub-project (e.g., meshmc, neozip, libnbtplusplus) |
| **Affected versions** | Version numbers or commit hashes |
| **Steps to reproduce** | Detailed reproduction steps |
| **Expected behavior** | What should happen |
| **Actual behavior** | What actually happens (crash, data leak, etc.) |
| **Impact assessment** | Your assessment of severity and exploitability |
| **Logs or crash reports** | Stack traces, core dumps, error messages |
| **Proof of concept** | Minimal reproducer (if available) |
| **Suggested fix** | If you have one |

### Example Report

```
Subject: [SECURITY] Buffer overflow in NeoZip deflate_fast

Affected component: neozip
Affected versions: All versions based on zlib-ng 2.x

Steps to reproduce:
1. Create a specially crafted gzip stream with [details]
2. Call inflate() with the crafted input
3. Observe buffer overflow at deflate_fast.c:42

Impact: Remote code execution via crafted compressed data.
Severity: Critical (CVSS 9.8)

PoC: Attached file crash_input.gz

Suggested fix: Add bounds check at deflate_fast.c:42 before
memcpy call.
```

---

## Disclosure Process

### Timeline

Project Tick follows a responsible disclosure process:

1. **Acknowledgment** — You will receive an acknowledgment within **48 hours**
   of your report.

2. **Triage** — The security team assesses severity and impact within
   **7 days**.

3. **Fix development** — A fix is developed privately. Timeline depends on
   severity:
   - **Critical (CVSS 9.0+):** Fix within **7 days**
   - **High (CVSS 7.0–8.9):** Fix within **14 days**
   - **Medium (CVSS 4.0–6.9):** Fix within **30 days**
   - **Low (CVSS 0.1–3.9):** Fix within **90 days**

4. **Coordinated disclosure** — The fix is released, and the vulnerability is
   disclosed publicly. Credit is given to the reporter (unless anonymity is
   requested).

5. **Advisory publication** — A security advisory is published on GitHub with
   the CVE ID (if assigned).

### Embargo

During the fix development period:

- Details of the vulnerability are kept confidential
- Only the core maintainers and the reporter have access
- Pre-disclosure to downstream distributors may occur for critical issues
- The reporter is asked not to disclose until the fix is released

---

## Supported Components

### Security-Critical Components

The following components handle untrusted input and are considered
security-critical:

| Component | Risk Area | Threat Model |
|-----------|-----------|--------------|
| **neozip** | Compression/decompression | Crafted compressed streams (e.g., zip bombs, buffer overflows) |
| **libnbtplusplus** | Binary data parsing | Malicious NBT files from untrusted sources |
| **json4cpp** | JSON parsing | Crafted JSON input (e.g., deeply nested objects, huge numbers) |
| **tomlplusplus** | TOML parsing | Crafted TOML configuration files |
| **cmark** | Markdown parsing | Crafted Markdown (e.g., pathological regex, huge nesting) |
| **genqrcode** | QR code encoding | Crafted encoding input |
| **meshmc** | Application | Network input (OAuth, HTTP APIs), file parsing, mod loading |
| **forgewrapper** | Java runtime | Classpath manipulation, installer extraction |
| **cgit** | Web interface | HTTP request handling, repository traversal |
| **mnv** | Text editor | Modeline parsing, file format handling |
| **corebinutils** | System utilities | Command-line input, file operations |
| **tickborg** | CI bot | AMQP messages, GitHub API responses |
| **meta** | Metadata generation | Upstream API responses (Mojang, Forge, etc.) |

### Fuzz Testing Coverage

Several sub-projects maintain active fuzz testing:

| Component | Fuzz Infrastructure | CI Workflow |
|-----------|-------------------|-------------|
| neozip | OSS-Fuzz, custom fuzzers | `neozip-fuzz.yml` |
| json4cpp | OSS-Fuzz, custom fuzzers | `json4cpp-fuzz.yml` |
| cmark | Custom fuzzers in `fuzz/` | `cmark-fuzz.yml` |
| tomlplusplus | Custom fuzzers in `fuzzing/` | `tomlplusplus-fuzz.yml` |

### Static Analysis Coverage

| Component | Tool | CI Workflow |
|-----------|------|-------------|
| meshmc | CodeQL | `meshmc-codeql.yml` |
| mnv | CodeQL, Coverity | `mnv-codeql.yml`, `mnv-coverity.yml` |
| neozip | CodeQL | `neozip-codeql.yml` |
| json4cpp | Semgrep, Flawfinder | `json4cpp-semgrep.yml`, `json4cpp-flawfinder.yml` |

---

## Security Practices

### Compiler Hardening

MeshMC's build system enables several hardening flags:

```cmake
# Stack protection
-fstack-protector-strong --param=ssp-buffer-size=4

# Buffer overflow detection
-O3 -D_FORTIFY_SOURCE=2

# Comprehensive warnings
-Wall -pedantic

# Position-independent code (ASLR support)
CMAKE_POSITION_INDEPENDENT_CODE ON
```

### Supply Chain Security

1. **Pinned Dependencies**
   - Nix inputs are content-addressed and locked in `flake.lock`
   - CI Nixpkgs revision is pinned in `ci/pinned.json` with SHA256 hashes
   - GitHub Actions use SHA-pinned action references

2. **Runner Hardening**
   - CI workflows use `step-security/harden-runner` with egress auditing
   - `repo-scorecards.yml` tracks OpenSSF Scorecard compliance
   - `repo-dependency-review.yml` scans dependency changes for known
     vulnerabilities

3. **Code Signing**
   - Release artifacts are signed
   - Git commits can be GPG/SSH signed (recommended but not required)

4. **CODEOWNERS Enforcement**
   - The `codeowners-validator` tool (built from source in `ci/`) validates
     the `CODEOWNERS` file to ensure all paths have designated reviewers

5. **GitHub Actions Security**
   - `zizmor` scans workflows for security issues
   - `actionlint` validates workflow syntax
   - Minimal permissions (`contents: read` by default)

### Network Security (MeshMC)

MeshMC handles network operations for:
- OAuth2 authentication (Microsoft account login via Qt6 NetworkAuth)
- HTTP APIs (Mojang, Forge, Fabric, Quilt, Modrinth, CurseForge)
- File downloads (game assets, mods, Java runtimes)

Security measures:
- TLS/HTTPS enforced for all network connections
- Certificate validation via Qt's SSL stack
- Download integrity verification (SHA-1, SHA-256 checksums)
- No execution of downloaded code without user consent

### Infrastructure Security

The Code of Conduct (Section 4.2) explicitly prohibits:

- Intentional submission of malicious code
- Supply-chain compromise attempts
- Infrastructure abuse, including CI/CD exploitation or service disruption
- License violations or intentional misattribution

Violations are treated as serious misconduct and may result in immediate
and permanent bans.

---

## Vulnerability History

Security advisories are published on the GitHub repository's Security tab:

```
https://github.com/Project-Tick/Project-Tick/security/advisories
```

---

## Third-Party Component Security

Since Project Tick includes forks of upstream projects (zlib-ng, nlohmann/json,
toml++, libqrencode, Vim, cgit, ofborg), security vulnerabilities in upstream
projects may affect Project Tick.

### Monitoring

- Upstream security advisories are monitored
- Dependabot alerts are enabled for Cargo, npm, and pip dependencies
- The `repo-dependency-review.yml` workflow checks for known vulnerabilities
  in dependency changes

### Patching Policy

- **Critical upstream vulnerabilities** — Patches are applied within 48 hours
  and backported to all supported release branches
- **High upstream vulnerabilities** — Patches applied within 7 days
- **Other upstream vulnerabilities** — Incorporated in the next regular sync

### Upstream Tracking

| Component | Upstream | Tracking |
|-----------|----------|----------|
| neozip | zlib-ng/zlib-ng | GitHub releases, OSS-Fuzz |
| json4cpp | nlohmann/json | GitHub releases, OSS-Fuzz |
| tomlplusplus | marzer/tomlplusplus | GitHub releases |
| cmark | commonmark/cmark | GitHub releases |
| genqrcode | fukuchi/libqrencode | GitHub releases |
| mnv | vim/vim | GitHub security advisories |
| cgit | zx2c4/cgit | Mailing list |
| ofborg/tickborg | NixOS/ofborg | GitHub releases |

---

## Contact

For security-related inquiries:

| Channel | Address |
|---------|---------|
| Security reports | [projecttick@projecttick.org](mailto:projecttick@projecttick.org) |
| General inquiries | [projecttick@projecttick.org](mailto:projecttick@projecttick.org) |
| Trademark | [yongdohyun@projecttick.org](mailto:yongdohyun@projecttick.org) |

**Do not use GitHub issues for security reports.**
