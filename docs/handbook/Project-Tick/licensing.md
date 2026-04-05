# Project Tick — Licensing

## Overview

Project Tick is a multi-licensed ecosystem. Because the monorepo contains
components with diverse origins — from BSD utility ports to GPL-licensed
applications to MIT/Zlib libraries — each sub-project carries the license
appropriate to its upstream lineage and Project Tick's own contributions.

The project uses the [REUSE](https://reuse.software/) specification (version
3.0) for license compliance. Every file in the repository is annotated with
SPDX license identifiers and copyright statements, either inline in file
headers or via the `REUSE.toml` configuration file.

---

## License Inventory

The `LICENSES/` directory contains 20 distinct SPDX-compliant license texts:

| SPDX Identifier | License Name | Category |
|-----------------|-------------|----------|
| `Apache-2.0` | Apache License 2.0 | Permissive |
| `BSD-1-Clause` | BSD 1-Clause License | Permissive |
| `BSD-2-Clause` | BSD 2-Clause "Simplified" License | Permissive |
| `BSD-3-Clause` | BSD 3-Clause "New" License | Permissive |
| `BSD-4-Clause` | BSD 4-Clause "Original" License | Permissive |
| `BSL-1.0` | Boost Software License 1.0 | Permissive |
| `CC-BY-SA-4.0` | Creative Commons Attribution-ShareAlike 4.0 | Creative Commons |
| `CC0-1.0` | Creative Commons Zero 1.0 Universal | Public Domain Dedication |
| `GPL-2.0-only` | GNU General Public License v2.0 only | Copyleft |
| `GPL-3.0-only` | GNU General Public License v3.0 only | Copyleft |
| `GPL-3.0-or-later` | GNU General Public License v3.0 or later | Copyleft |
| `LGPL-2.0-or-later` | GNU Lesser General Public License v2.0 or later | Weak Copyleft |
| `LGPL-2.1-or-later` | GNU Lesser General Public License v2.1 or later | Weak Copyleft |
| `LGPL-3.0-or-later` | GNU Lesser General Public License v3.0 or later | Weak Copyleft |
| `LicenseRef-Qt-Commercial` | Qt Commercial License (reference) | Proprietary |
| `MIT` | MIT License | Permissive |
| `MS-PL` | Microsoft Public License | Permissive |
| `Unlicense` | The Unlicense | Public Domain Dedication |
| `Vim` | Vim License | Permissive (custom) |
| `Zlib` | zlib License | Permissive |

---

## Per-Component License Map

### Applications

| Component | Directory | License | Copyright |
|-----------|-----------|---------|-----------|
| **MeshMC** | `meshmc/` | GPL-3.0-or-later | 2026 Project Tick |
| MeshMC (historical code) | `meshmc/` | Apache-2.0 (incorporated work) | 2012–2022 MultiMC Contributors |
| **MNV** | `mnv/` | Vim AND GPL-3.0-or-later | Bram Moolenaar & Vim Contributors & Project Tick |
| **cgit** | `cgit/` | GPL-2.0-only | cgit Contributors & Project Tick |

### Libraries

| Component | Directory | License | Copyright |
|-----------|-----------|---------|-----------|
| **NeoZip** | `neozip/` | Zlib | Zlib Contributors & Zlib-ng Contributors & Project Tick |
| **Json4C++** | `json4cpp/` | MIT | Json4C++ Contributors & Project Tick |
| **toml++** | `tomlplusplus/` | MIT | Toml++ Contributors & Project Tick |
| **libnbt++** | `libnbtplusplus/` | LGPL-3.0-or-later | libnbtplusplus Contributors & ljfa-ag & Project Tick |
| **cmark** | `cmark/` | BSD-2-Clause AND MIT AND CC-BY-SA-4.0 | CMark Contributors & Project Tick |
| **GenQRCode** | `genqrcode/` | LGPL-2.1-or-later | GenQRCode Contributors & Project Tick |
| **ForgeWrapper** | `forgewrapper/` | MIT | ForgeWrapper Contributors & Project Tick |

### System Utilities

| Component | Directory | License | Copyright |
|-----------|-----------|---------|-----------|
| **CoreBinUtils** | `corebinutils/` | BSD-1-Clause AND BSD-2-Clause AND BSD-3-Clause AND BSD-4-Clause AND MIT | FreeBSD Contributors & Project Tick |

### Infrastructure

| Component | Directory | License | Copyright |
|-----------|-----------|---------|-----------|
| **Meta** | `meta/` | MS-PL | MultiMC Contributors & PolyMC Contributors & PrismLauncher Contributors & Project Tick |
| **tickborg** | `ofborg/` | MIT | NixOS Contributors & Project Tick |
| **Images4Docker** | `images4docker/` | GPL-3.0-or-later | Project Tick |

### Archived

| Component | Directory | License | Copyright |
|-----------|-----------|---------|-----------|
| **ProjT Launcher** | `archived/projt-launcher/` | GPL-3.0-only | MultiMC Contributors & Prism Launcher Contributors & PolyMC Contributors & Project Tick |
| **ProjT Modpack** | `archived/projt-modpack/` | GPL-3.0-only | Project Tick |
| **ProjT Minicraft Modpack** | `archived/projt-minicraft-modpack/` | MIT | Project Tick |
| **ptlibzippy** | `archived/ptlibzippy/` | Zlib | Zlib Contributors & Project Tick |

---

## REUSE.toml Analysis

The `REUSE.toml` file (version 1) uses `[[annotations]]` blocks to map file
paths to their SPDX license identifiers and copyright statements. This is the
primary mechanism for bulk license annotation.

### Infrastructure and Configuration Files

```toml
[[annotations]]
path = [
  ".gitignore", ".gitattributes", ".gitmodules", ".github/**",
  ".envrc", ".markdownlint.yaml", ".markdownlintignore",
  "Containerfile", "default.nix", "flake.lock", "flake.nix",
  "shell.nix", "vcpkg-configuration.json", "vcpkg.json",
  ".clang-format", ".clang-tidy", "CODEOWNERS", "hooks/**", "ci/**"
]
SPDX-License-Identifier = "CC0-1.0"
SPDX-FileCopyrightText = "NONE"
```

Configuration files, CI scripts, Git metadata, and build system configuration
are placed in the public domain under CC0-1.0 with no copyright claim.

### Documentation

```toml
[[annotations]]
path = ["**/*.md", "doc/**"]
SPDX-License-Identifier = "CC0-1.0"
SPDX-FileCopyrightText = "2026 Project Tick"
```

All Markdown files and documentation are CC0-1.0, allowing unrestricted reuse.

### MeshMC-Specific Files

```toml
# Launcher packaging
path = ["launcher/package/**"]
SPDX-License-Identifier = "GPL-3.0-or-later"

# Qt UI files
path = ["**/*.ui"]
SPDX-License-Identifier = "GPL-3.0-or-later"

# CMake presets
path = ["CMakePresets.json"]
SPDX-License-Identifier = "GPL-3.0-or-later"

# Nix build files
path = ["nix/**"]
SPDX-License-Identifier = "GPL-3.0-or-later"

# Branding and resources
path = ["branding/**", "launcher/resources/**"]
SPDX-License-Identifier = "CC0-1.0"
```

### CMake Build Files

```toml
path = ["cmake/**", "**/CMakeLists.txt"]
SPDX-License-Identifier = "BSD-3-Clause"
SPDX-FileCopyrightText = "Various authors"
```

CMake modules and build definitions use BSD-3-Clause, reflecting their diverse
authorship.

### Test Data

```toml
path = ["**/testdata/**"]
SPDX-License-Identifier = "CC0-1.0"
SPDX-FileCopyrightText = "NONE"
```

Test data has no copyright claims and is in the public domain.

---

## License Compatibility

### Core Dependency Chain

MeshMC (GPL-3.0-or-later) links against libraries with the following licenses:

| Library | License | GPL-3.0 Compatible? |
|---------|---------|---------------------|
| json4cpp | MIT | Yes — permissive |
| tomlplusplus | MIT | Yes — permissive |
| libnbtplusplus | LGPL-3.0-or-later | Yes — LGPL is GPL-compatible |
| neozip | Zlib | Yes — permissive |
| cmark | BSD-2-Clause/MIT | Yes — permissive |
| genqrcode | LGPL-2.1-or-later | Yes — LGPL is GPL-compatible |
| Qt 6 | LGPL-3.0 / GPL-3.0 / Commercial | Yes — LGPL/GPL-compatible |
| QuaZip | LGPL-2.1 | Yes — LGPL is GPL-compatible |
| libarchive | BSD-2-Clause | Yes — permissive |
| ECM | BSD-3-Clause | Yes — permissive |

All library dependencies are GPL-3.0 compatible. The GPL-3.0-or-later license
of MeshMC governs the combined work.

### ForgeWrapper (Runtime)

ForgeWrapper (MIT) is loaded at runtime as a separate Java process, not linked
at compile time. The MIT license is compatible with GPL-3.0 for distribution
purposes, and runtime invocation does not create a derivative work concern.

### Meta (MS-PL)

The MS-PL (Microsoft Public License) used by `meta/` is a permissive license
that allows use, modification, and redistribution. It is generally considered
compatible with GPL for independent components. Since `meta/` is a standalone
Python project that generates JSON data consumed by MeshMC over HTTP, there is
no linking relationship.

### CoreBinUtils (Multi-BSD)

CoreBinUtils uses a combination of BSD-1-Clause, BSD-2-Clause, BSD-3-Clause,
BSD-4-Clause, and MIT — reflecting the diverse origins of FreeBSD utilities.
The BSD-4-Clause (advertising clause) applies only to the specific files that
carry it. All BSD variants are permissive and do not impose copyleft
obligations.

### MNV (Vim License + GPL-3.0-or-later)

MNV uses a dual license: the Vim license (a permissive custom license) and
GPL-3.0-or-later. The Vim license is similar to the Charityware license and
allows free use, modification, and redistribution. The GPL-3.0-or-later
applies to Project Tick's modifications.

### cgit (GPL-2.0-only)

cgit uses GPL-2.0-only (not "or later"), which means it cannot be
relicensed under GPL-3.0. It remains an independent component with no
linking relationship to GPL-3.0 components.

---

## SPDX Headers

### Inline Headers

Source files should include SPDX headers at the top:

```c
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: GPL-3.0-or-later
```

```python
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-License-Identifier: MS-PL
```

```cmake
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-License-Identifier: BSD-3-Clause
```

### REUSE.toml Bulk Annotations

For files where inline headers are impractical (binary files, generated files,
configuration files), use `REUSE.toml` annotations with glob patterns:

```toml
[[annotations]]
path = ["pattern/**"]
SPDX-License-Identifier = "LICENSE-ID"
SPDX-FileCopyrightText = "Copyright holder"
```

### Checking Compliance

```bash
# Install reuse tool
pip install reuse

# Check entire repository
reuse lint

# Download missing license texts
reuse download --all
```

The pre-commit hook via lefthook automatically runs `reuse lint` and
downloads missing licenses if needed.

---

## Adding New Files

When adding new files to the repository:

1. **Determine the appropriate license** based on the sub-project:
   - Files in `meshmc/` → GPL-3.0-or-later
   - Files in `neozip/` → Zlib
   - Files in `json4cpp/` → MIT
   - Files in `meta/` → MS-PL
   - Documentation → CC0-1.0
   - Configuration/build files → CC0-1.0 or BSD-3-Clause
   - Test data → CC0-1.0

2. **Add SPDX headers** to the file (if it supports comments)

3. **Or add a REUSE.toml annotation** for files without comment support

4. **Run `reuse lint`** to verify compliance

### Adding New Sub-Projects

If adding an entirely new sub-project:

1. Add a `[[annotations]]` block to `REUSE.toml` for the new directory
2. Place the appropriate license text in `LICENSES/` if not already present
3. Ensure all files have proper SPDX identifiers
4. Document the license in the sub-project's README

---

## Third-Party License Obligations

### Attribution Requirements

Several licenses in the ecosystem require attribution in distributed binaries:

| License | Attribution Requirement |
|---------|----------------------|
| Apache-2.0 | NOTICE file, license text |
| BSD-2-Clause | License text in documentation |
| BSD-3-Clause | License text in documentation |
| BSD-4-Clause | License text + advertising clause |
| MIT | License text |
| LGPL-2.1/3.0 | License text, source availability |
| GPL-2.0/3.0 | Full source code availability |
| MS-PL | License text |
| CC-BY-SA-4.0 | Attribution, ShareAlike |

### Copyleft Obligations

| License | Source Obligation | Dynamic Linking |
|---------|------------------|-----------------|
| GPL-2.0-only | Full source for the program | Derivative work |
| GPL-3.0-only/or-later | Full source for the program | Derivative work |
| LGPL-2.1-or-later | Source for LGPL portions, object files for relinking | Permitted without GPL |
| LGPL-3.0-or-later | Source for LGPL portions, installation info | Permitted without GPL |

---

## Trademark vs. License

It is crucial to understand that **open source licenses do not grant trademark
rights**. As stated in `TRADEMARK.md`:

> Open source licenses govern the use, modification, and redistribution of
> source code only. They do **not** grant rights to use the Project Tick name,
> logo, or branding.

See [trademark-policy.md](trademark-policy.md) for the full trademark policy.

---

## CLA and License Grants

The Project Tick Contributor License Agreement (CLA) ensures that all
contributions can be distributed under the project's existing licenses. By
signing the CLA, contributors:

1. Confirm they have the legal right to make the contribution
2. Grant Project Tick a perpetual license to distribute the contribution
3. Agree not to knowingly infringe third-party rights

This allows Project Tick to maintain license consistency across the ecosystem
without requiring future relicensing negotiations.

CLA text: <https://projecttick.org/licenses/PT-CLA-2.0.txt>
