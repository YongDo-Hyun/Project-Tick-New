# Project Tick — Organization Overview

## Introduction

Project Tick is a modular software ecosystem organized as a unified monorepo. It
encompasses applications, libraries, system utilities, infrastructure tooling,
and metadata generators — all managed under a single repository at
`github.com/Project-Tick/Project-Tick`. The project is dedicated to providing
developers with ease of use and users with long-lasting software.

The monorepo approach ensures tight integration between components while
preserving the independence of each sub-project. Every directory at the
repository root represents an autonomous module, library, tool, or application
that can be built and used standalone or as part of the larger system.

Project Tick focuses on three guiding principles:

1. **Reproducible builds** — Nix flakes and pinned dependencies ensure every
   build produces identical output regardless of the host environment.
2. **Minimal dependencies** — Each component pulls only what it strictly needs.
3. **Full control over the software stack** — From compression libraries to text
   editors, Project Tick maintains its own forks and adaptations to guarantee
   long-term stability and security.

---

## Mission

Project Tick exists to build, package, and run software across multiple
platforms with complete transparency and reproducibility. The project provides:

- A custom Minecraft launcher (MeshMC) with deep mod-loader integration
- System-level UNIX utilities ported from FreeBSD
- A text editor fork (MNV) with modern enhancements
- Foundational C/C++ libraries for compression, serialization, and parsing
- Infrastructure for CI/CD, container images, and metadata generation
- Git web interfaces and documentation tooling

Every component feeds up into the broader mission: an ecosystem where every
dependency is accounted for, every license is tracked, and every build is
reproducible.

---

## Sub-Projects

Project Tick contains the following top-level components, organized by category.

### Applications

| Directory | Name | Description | Language | License |
|-----------|------|-------------|----------|---------|
| `meshmc/` | **MeshMC** | Custom Minecraft launcher focused on predictability, long-term stability, and simplicity. Supports Forge, NeoForge, Fabric, and Quilt mod loaders. Built with Qt 6 and C++23. Current version: 7.0.0. | C++ | GPL-3.0-or-later |
| `mnv/` | **MNV** | Greatly improved fork of the Vi/Vim text editor. Features multi-level undo, syntax highlighting, command-line history, spell checking, filename completion, block operations, and a script language. Provides a POSIX-compatible vi implementation in its minimal build. | C | Vim AND GPL-3.0-or-later |
| `cgit/` | **cgit** | Fast CGI-based web interface for Git repositories. Uses a built-in cache to decrease server I/O pressure. Supports Lua scripting for custom filters. | C | GPL-2.0-only |

### Libraries

| Directory | Name | Description | Language | License |
|-----------|------|-------------|----------|---------|
| `neozip/` | **NeoZip** | Next-generation zlib/zlib-ng fork for data compression. Features SIMD-accelerated implementations (SSE2, AVX2, AVX-512, NEON, etc.) for Adler32, CRC32, inflate, and deflate. Supports CPU intrinsics on x86-64, ARM, Power, RISC-V, LoongArch, and s390x. | C | Zlib |
| `json4cpp/` | **Json4C++** | Header-only JSON library for modern C++ (nlohmann/json fork). Supports JSON Pointer, JSON Patch, JSON Merge Patch, BSON, CBOR, MessagePack, UBJSON, and BJData binary formats. Single-header and multi-header modes. | C++ | MIT |
| `tomlplusplus/` | **toml++** | Header-only TOML parser and serializer for C++17. Passes all tests in the official toml-test suite. Supports serialization to JSON and YAML, proper UTF-8 handling, and works with or without exceptions and RTTI. | C++ | MIT |
| `libnbtplusplus/` | **libnbt++** | C++ library for Minecraft's Named Binary Tag (NBT) file format. Reads and writes compressed and uncompressed NBT files. Version 3 is a ground-up rewrite for usability. | C++ | LGPL-3.0-or-later |
| `cmark/` | **cmark** | CommonMark reference implementation in C. Provides a C API for parsing and rendering Markdown documents. Includes fuzz testing infrastructure. | C | BSD-2-Clause AND MIT AND CC-BY-SA-4.0 |
| `genqrcode/` | **GenQRCode** | QR Code encoding library (libqrencode fork). Supports QR Code model 2 per JIS X0510:2004 / ISO/IEC 18004. Handles numeric, alphanumeric, kanji (Shift-JIS), and 8-bit data. Also supports Micro QR Code (experimental). | C | LGPL-2.1-or-later |
| `forgewrapper/` | **ForgeWrapper** | Java library enabling launchers to start Minecraft 1.13+ with Forge. Provides a service-provider interface (`IFileDetector`) for custom file detection rules. Built with Gradle. | Java | MIT |

### System Utilities

| Directory | Name | Description | Language | License |
|-----------|------|-------------|----------|---------|
| `corebinutils/` | **CoreBinUtils** | Collection of BSD/FreeBSD core utilities ported to Linux. Includes `cat`, `chmod`, `cp`, `date`, `dd`, `df`, `echo`, `ed`, `expr`, `hostname`, `kill`, `ln`, `ls`, `mkdir`, `mv`, `nproc`, `ps`, `pwd`, `realpath`, `rm`, `rmdir`, `sh`, `sleep`, `stty`, `sync`, `test`, `timeout`, and more. Uses musl-first toolchain selection. | C | BSD-1-Clause AND BSD-2-Clause AND BSD-3-Clause AND BSD-4-Clause AND MIT |

### Infrastructure & Tooling

| Directory | Name | Description | Language | License |
|-----------|------|-------------|----------|---------|
| `meta/` | **Meta** | Metadata generator for the MeshMC launcher. Generates JSON manifests and JARs for Mojang, Forge, NeoForge, Fabric, Quilt, LiteLoader, and Java runtime versions. Written in Python, uses Poetry for dependency management. Deployable as a NixOS service. | Python | MS-PL |
| `ofborg/` | **tickborg** | Distributed RabbitMQ-based CI system adapted from NixOS ofborg. Automatically detects changed projects in PRs, builds affected sub-projects using their native build systems, posts results as GitHub check runs, and supports multi-platform builds (Linux, macOS, Windows, FreeBSD). | Rust | MIT |
| `images4docker/` | **Images4Docker** | Collection of 40 Dockerfiles for building MeshMC across different Linux distributions. Each image includes the Qt 6 toolchain and all MeshMC build dependencies. Supports apt, dnf, apk, zypper, yum, pacman, xbps, nix, and emerge package managers. Rebuilt daily at 03:17 UTC. | Dockerfile | GPL-3.0-or-later |
| `ci/` | **CI Infrastructure** | CI support files including Nix-based tooling (treefmt, codeowners-validator), GitHub Actions JavaScript helpers (commit linting, PR preparation, review management), branch classification, and pinned Nixpkgs for reproducible formatting. | Nix, JavaScript | CC0-1.0 |
| `hooks/` | **Git Hooks** | Lefthook-managed Git hooks for pre-commit REUSE license checking and code style validation via checkpatch. | Shell | CC0-1.0 |

### Archived Projects

| Directory | Name | Description | License |
|-----------|------|-------------|---------|
| `archived/projt-launcher/` | **ProjT Launcher** | Original Minecraft launcher (predecessor to MeshMC). Based on MultiMC/PrismLauncher/PolyMC. | GPL-3.0-only |
| `archived/projt-modpack/` | **ProjT Modpack** | Minecraft modpack distribution tooling. | GPL-3.0-only |
| `archived/projt-minicraft-modpack/` | **ProjT Minicraft Modpack** | Minicraft modpack distribution. | MIT |
| `archived/ptlibzippy/` | **ptlibzippy** | ZIP library (predecessor to NeoZip integration). | Zlib |

### Documentation & Configuration

| Directory / File | Description |
|------------------|-------------|
| `docs/` | Project documentation including the developer handbook |
| `LICENSES/` | SPDX-compliant license texts (20 distinct licenses) |
| `REUSE.toml` | REUSE 3.0 compliance annotations mapping paths to licenses |
| `flake.nix` | Top-level Nix flake providing development shells with LLVM 22 |
| `flake.lock` | Locked Nix inputs for reproducibility |
| `bootstrap.sh` | Cross-distro bootstrap script for dependency installation |
| `bootstrap.cmd` | Windows bootstrap script using Scoop and vcpkg |
| `lefthook.yml` | Git hooks configuration for pre-commit checks |
| `.github/` | GitHub Actions workflows, issue templates, PR template, CODEOWNERS, DCO enforcement |

---

## Technology Stack

### Programming Languages

| Language | Where Used |
|----------|------------|
| C | neozip, cmark, genqrcode, cgit, corebinutils, mnv |
| C++ (C++23) | meshmc, json4cpp (C++11/17), tomlplusplus (C++17), libnbtplusplus (C++11) |
| Rust | tickborg (ofborg) |
| Java | forgewrapper |
| Python | meta |
| JavaScript / Node.js | CI scripts (github-script) |
| Nix | CI infrastructure, development shells, NixOS deployment modules |
| Shell (Bash/POSIX) | bootstrap scripts, hooks, build orchestration |
| Dockerfile | images4docker |
| CMake | meshmc, neozip, cmark, genqrcode, json4cpp, libnbtplusplus, mnv |

### Build Systems

| Build System | Projects |
|--------------|----------|
| CMake | meshmc, neozip, cmark, genqrcode, json4cpp, libnbtplusplus, mnv |
| Meson | tomlplusplus |
| Make (GNU Make) | cgit, corebinutils |
| Autotools | mnv (configure), genqrcode (configure.ac), neozip (configure) |
| Gradle | forgewrapper |
| Cargo | tickborg |
| Poetry | meta |
| Nix | CI, development shells, deployment |

### Frameworks & Key Dependencies

| Dependency | Used By | Purpose |
|------------|---------|---------|
| Qt 6 (Core, Widgets, Concurrent, Network, NetworkAuth, Test, Xml) | meshmc | GUI framework |
| QuaZip (Qt 6) | meshmc | ZIP archive support |
| zlib / NeoZip | meshmc, neozip | Data compression |
| libarchive | meshmc | Archive extraction |
| Extra CMake Modules (ECM) | meshmc | KDE CMake utilities |
| RabbitMQ (AMQP) | tickborg | Message queue for distributed CI |
| Poetry | meta | Python dependency management |
| Crowdin | meshmc | Translation management |

---

## How Sub-Projects Relate

The Project Tick ecosystem has clear dependency chains:

```
meshmc (application)
├── json4cpp        (JSON parsing)
├── tomlplusplus    (TOML configuration parsing)
├── libnbtplusplus  (Minecraft NBT format)
├── neozip          (compression)
├── cmark           (Markdown rendering)
├── genqrcode       (QR code generation)
├── forgewrapper    (Forge mod loader integration)
└── meta            (version metadata feeds)

tickborg (CI system)
├── Detects changes across all sub-projects
├── Builds using native build systems
└── Posts results as GitHub check runs

images4docker (container images)
├── Provides build environments for meshmc CI
└── Covers 40 Linux distributions with Qt 6

corebinutils (standalone)
└── Independent FreeBSD utility ports

mnv (standalone)
└── Independent Vim fork

cgit (standalone)
└── Independent Git web interface
```

MeshMC is the primary consumer of the library sub-projects. The `meta/`
component generates the version metadata that MeshMC uses to discover and
download Minecraft versions, mod loaders, and Java runtimes. The `forgewrapper/`
component is a Java shim that MeshMC invokes at runtime to bootstrap Forge.

The `tickborg` system orchestrates CI across the entire monorepo, detecting
which sub-projects are affected by a given change and building only those
projects using their respective build systems.

---

## Repository Governance

Project Tick is maintained by its core contributors under the oversight of
Mehmet Samet Duman (trademark holder). The project uses:

- **CODEOWNERS** for ownership-based review routing
- **DCO (Developer Certificate of Origin)** sign-off on every commit
- **CLA (Contributor License Agreement)** for all contributions
- **Conventional Commits** for structured commit messages
- **REUSE 3.0** for license compliance

The Code of Conduct (Version 2, 15 February 2026) establishes behavioral and
ethical standards focused on technical integrity, licensing compliance,
infrastructure security, and good-faith collaboration.

---

## Official Communication Channels

| Channel | URL / Address |
|---------|---------------|
| GitHub Issues | `github.com/Project-Tick/Project-Tick/issues` |
| Email | `projecttick@projecttick.org` |
| Trademark inquiries | `yongdohyun@projecttick.org` |
| CLA text | `projecttick.org/licenses/PT-CLA-2.0.txt` |
| Crowdin (translations) | `crowdin.com/project/projtlauncher` |

---

## Version History

Project Tick evolved from several predecessor projects:

1. **MultiMC** (2012–2022) — The original custom Minecraft launcher. Apache-2.0
   licensed. MeshMC's launcher code incorporates work from this project.

2. **PolyMC / PrismLauncher** — Community forks of MultiMC. The `meta/`
   component traces its lineage through these projects (MS-PL license).

3. **ProjT Launcher** — Project Tick's first launcher, now archived in
   `archived/projt-launcher/`. GPL-3.0-only.

4. **MeshMC** — The current-generation launcher, a ground-up evolution with
   C++23, Qt 6, and the full library stack.

The infrastructure components have diverse origins:

- **tickborg** is adapted from NixOS's [ofborg](https://github.com/NixOS/ofborg)
- **neozip** is based on [zlib-ng](https://github.com/zlib-ng/zlib-ng)
- **json4cpp** is based on [nlohmann/json](https://github.com/nlohmann/json)
- **tomlplusplus** is based on [marzer/tomlplusplus](https://github.com/marzer/tomlplusplus)
- **cgit** is the Project Tick fork of the cgit Git web interface
- **cmark** is the Project Tick fork of the CommonMark reference implementation
- **mnv** is the Project Tick fork of Vim
- **corebinutils** contains FreeBSD utility ports

---

## Platform Support

### MeshMC (Primary Application)

| Platform | Architecture | Status |
|----------|-------------|--------|
| Linux | x86_64 | Fully supported |
| Linux | aarch64 | Fully supported |
| macOS | x86_64 | Fully supported |
| macOS | aarch64 (Apple Silicon) | Fully supported |
| macOS | Universal Binary | Supported via `macos_universal` preset |
| Windows | x86_64 (MSVC) | Fully supported |
| Windows | x86_64 (MinGW) | Fully supported |
| Windows | aarch64 | Supported |
| FreeBSD | x86_64 | Supported (VM-based CI) |

### MNV

MNV runs on MS-Windows (7, 8, 10, 11), macOS, Haiku, VMS, and almost all
flavors of UNIX.

### CoreBinUtils

Targets Linux with musl-first toolchain selection. Also builds against glibc
when musl is unavailable.

### tickborg CI Platform Matrix

| Platform | Runner |
|----------|--------|
| `x86_64-linux` | `ubuntu-latest` |
| `aarch64-linux` | `ubuntu-24.04-arm` |
| `x86_64-darwin` | `macos-15` |
| `aarch64-darwin` | `macos-15` |
| `x86_64-windows` | `windows-2025` |
| `aarch64-windows` | `windows-2025` |
| `x86_64-freebsd` | `ubuntu-latest` (VM) |

---

## Quick Links

| Resource | Path |
|----------|------|
| Root README | `README.md` |
| Contributing Guide | `CONTRIBUTING.md` |
| Security Policy | `SECURITY.md` |
| Code of Conduct | `CODE_OF_CONDUCT.md` |
| Trademark Policy | `TRADEMARK.md` |
| License Directory | `LICENSES/` |
| REUSE Configuration | `REUSE.toml` |
| MeshMC Build Guide | `meshmc/BUILD.md` |
| CI Infrastructure | `ci/README.md` |
| tickborg Documentation | `ofborg/README.md` |
| Developer Handbook | `docs/handbook/` |

---

## Naming Conventions

- **Project Tick** — The umbrella organization and monorepo name
- **MeshMC** — The Minecraft launcher application
- **MNV** — The text editor (Vi/Vim fork)
- **tickborg** — The distributed CI bot (the Rust workspace in `ofborg/tickborg/`)
- **ofborg** — The upstream project tickborg is derived from; also the directory name (`ofborg/`)
- **NeoZip** — The compression library (zlib-ng fork)
- **Json4C++** — The JSON library (nlohmann/json fork)
- **toml++** — The TOML library
- **libnbt++** — The NBT library
- **GenQRCode** — The QR code library (libqrencode fork)
- **ForgeWrapper** — The Forge bootstrapper
- **CoreBinUtils** — The BSD utility ports
- **Meta** — The launcher metadata generator
- **Images4Docker** — The Docker image collection
- **cgit** — The Git web interface

The trademark "Project Tick" and associated branding are owned by Mehmet Samet
Duman. See `TRADEMARK.md` for usage policies.
