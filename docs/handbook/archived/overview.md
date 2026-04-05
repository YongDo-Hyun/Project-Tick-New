# Archived Projects — Overview

## Purpose

The `archived/` directory contains legacy Project Tick projects that are no longer
actively developed. These projects remain in the monorepo for historical reference,
documentation completeness, and potential future reuse of components.

Archived projects are not built, tested, or deployed by the current CI pipeline.
They are preserved as-is at the time of archival.

---

## Archived Projects

| Directory                  | Project Name         | Type                   | License  | Status      |
|---------------------------|---------------------|------------------------|----------|-------------|
| `archived/projt-launcher/` | ProjT Launcher      | Minecraft Launcher (C++/Qt) | GPL-3.0 | Archived    |
| `archived/projt-modpack/`  | ProjT Modpack       | Minecraft Modpack      | GPL-3.0  | Archived    |
| `archived/projt-minicraft-modpack/` | MiniCraft Modpack | Minecraft Modpack Collection | MIT | Archived |
| `archived/ptlibzippy/`     | PTlibzippy          | Compression Library (C)| zlib License | Archived |

---

## Why Projects Are Archived

Projects are moved to `archived/` when they meet one or more of these criteria:

1. **Superseded by a newer project** — The functionality has been replaced by a different
   component in the monorepo (e.g., ProjT Launcher was the standalone launcher before
   MeshMC took over as the primary launcher)
2. **No longer maintained** — The project has reached end-of-life and no further
   development is planned
3. **Completed scope** — The project achieved its intended purpose and doesn't need
   ongoing changes (e.g., modpack archives)
4. **Consolidation** — Standalone repositories were merged into the monorepo as
   subtrees, and the project's active development has ended

---

## Project Summaries

### ProjT Launcher (`archived/projt-launcher/`)

ProjT Launcher was a structurally disciplined Minecraft launcher fork of Prism Launcher.
It was engineered for long-term maintainability, architectural clarity, and controlled
ecosystem evolution.

**Key characteristics**:
- Written in C++23 with Qt 6
- CMake build system with presets for Linux, macOS, Windows (MSVC and MinGW)
- Layered architecture: UI (Qt Widgets) → Core/Domain → Tasks → Networking
- Detached fork libraries: zlib, bzip2, quazip, cmark, tomlplusplus, libqrencode, libnbtplusplus
- Nix-based CI and reproducible builds
- Containerized build support (Dockerfile/Containerfile)
- Comprehensive documentation in `docs/` and `docs/handbook/`

**Notable features at time of archival**:
- Launcher Hub (web-based dashboard using CEF on Linux, native on Windows/macOS)
- Modrinth collection import
- Fabric/Quilt/NeoForge mod loader support
- Java runtime auto-detection and management
- Multi-platform packaging: RPM, DEB, AppImage, Flatpak, macOS App Bundle, Windows MSI

**Last known version**: 0.0.5-1 (draft)

**License heritage**: GPL-3.0, with upstream license blocks from Prism Launcher
(GPL-3.0), PolyMC (GPL-3.0), and MultiMC (Apache-2.0).

For full documentation, see [projt-launcher.md](projt-launcher.md).

---

### ProjT Modpack (`archived/projt-modpack/`)

ProjT Modpack was a Minecraft modpack curated by Project Tick. The project contained
modpack configuration files and promotional assets.

**Key characteristics**:
- Licensed under GPL-3.0
- Contains promotional images (ProjT1.png, ProjT2.png, ProjT3.png)
- Affiliate banner assets (affiliate-banner-bg.webp, affiliate-banner-fg.webp)
- Minimal README — the modpack itself was distributed through launcher platforms

**Status**: Archived with no active maintenance. The modpack distribution was
handled through the ProjT Launcher and mod platform integrations (Modrinth,
CurseForge).

For full documentation, see [projt-modpack.md](projt-modpack.md).

---

### MiniCraft Modpack (`archived/projt-minicraft-modpack/`)

The MiniCraft Modpack is a historical archive of Minecraft modpack releases
organized into multiple "seasons" (S1 through S4). This is a collection of
pre-built modpack ZIP files rather than a source code project.

**Key characteristics**:
- Licensed under MIT
- Organized by season:
  - **MiniCraft S1**: Versions from 12.1.5 through 13.0.0, including beta/alpha/pre-release builds
  - **MiniCraft S2**: Versions with mixed naming (A00051c74C, L3.0, R10056a75A, N1.0, N2.0)
  - **MiniCraft S3**: Versions from 1.0 through 1.2.0.3, plus a DEV-1.2 build
  - **MiniCraft S4**: Alpha versions (0.0.1–0.0.3), Beta versions (0.1–0.2.1), and
    releases including a "LASTMAJORRELEASE-2.0.0"
- Contains compiled ZIP archives, not source code

**Archive purpose**: Preserves the complete release history of the MiniCraft
modpack series for historical reference.

---

### PTlibzippy (`archived/ptlibzippy/`)

PTlibzippy is a Project Tick fork of the zlib compression library, version 0.0.5.1.
It's a general-purpose lossless data compression library implementing the DEFLATE
algorithm (RFC 1950, 1951, 1952).

**Key characteristics**:
- Written in C
- CMake and Autotools (configure/Makefile) build systems
- Bazel build support (BUILD.bazel, MODULE.bazel)
- Extensive cross-platform support (Unix, Windows, Amiga, OS/400, QNX, VMS)
- Thread-safe implementation
- Custom PNG shim layer (`ptlibzippy_pngshim.c`) for libpng integration
- Prefix support for symbol namespacing (`PTLIBZIPPY_PREFIX`)
- Language bindings: Ada, C#/.NET, Delphi, Python, Perl, Java, Tcl

**License**: zlib license (permissive, compatible with GPL)

**Why forked**: The fork was maintained to resolve symbol conflicts when bundling
zlib alongside libpng in the ProjT Launcher. The custom `ptlibzippy_pngshim.c`
and symbol prefixing prevented linker conflicts in the launcher's build.

For full documentation, see [ptlibzippy.md](ptlibzippy.md).

---

## Directory Structure

```
archived/
├── projt-launcher/              # ProjT Launcher (C++/Qt Minecraft Launcher)
│   ├── CMakeLists.txt           # Root CMake build file
│   ├── CMakePresets.json        # Build presets (linux, macos, windows_msvc, windows_mingw)
│   ├── Containerfile            # Docker/Podman build container
│   ├── CHANGELOG.md             # Release changelog
│   ├── COPYING.md               # License (GPL-3.0 + upstream notices)
│   ├── MAINTAINERS              # Maintainer contact info
│   ├── README                   # Project overview and build instructions
│   ├── default.nix              # Nix build via flake-compat
│   ├── bootstrap/               # Platform bootstrapping (macOS)
│   ├── buildconfig/             # Build configuration templates
│   ├── ci/                      # CI infrastructure (own copy, pre-monorepo)
│   ├── cmake/                   # CMake modules and vcpkg integration
│   ├── docs/                    # Developer and user documentation
│   │   ├── architecture/        # Architecture overview
│   │   ├── contributing/        # Contributing guides
│   │   └── handbook/            # User/developer handbook
│   └── ...
├── projt-modpack/               # ProjT Modpack
│   ├── COPYING.md               # License (GPL-3.0)
│   ├── LICENSE                  # GPL-3.0 full text
│   ├── README.md                # Minimal README
│   └── *.png, *.webp            # Promotional assets
├── projt-minicraft-modpack/     # MiniCraft Modpack Archive
│   ├── LICENSE                  # MIT License
│   ├── README.md                # Minimal README
│   └── MiniCraft/               # Season-organized modpack ZIPs
│       ├── MiniCraft S1/        # Season 1 releases
│       ├── MiniCraft S2/        # Season 2 releases
│       ├── MiniCraft S3/        # Season 3 releases
│       └── MiniCraft S4/        # Season 4 releases
└── ptlibzippy/                  # PTlibzippy (zlib fork)
    ├── CMakeLists.txt           # CMake build system
    ├── BUILD.bazel              # Bazel build
    ├── MODULE.bazel             # Bazel module definition
    ├── Makefile.in              # Autotools Makefile template
    ├── configure                # Autotools configure script
    ├── COPYING.md               # zlib license
    ├── README                   # Library overview
    ├── README-cmake.md          # CMake build instructions
    ├── FAQ                      # Frequently asked questions
    ├── INDEX                    # File listing
    ├── ptlibzippy.h             # Public API header
    ├── ptzippyconf.h            # Configuration header
    ├── adler32.c                # Adler-32 checksum
    ├── compress.c               # Compression API
    ├── crc32.c                  # CRC-32 checksum
    ├── deflate.c                # DEFLATE compression
    ├── inflate.c                # DEFLATE decompression
    ├── ptlibzippy_pngshim.c     # PNG integration shim
    ├── ptzippyutil.c            # Internal utilities
    └── contrib/                 # Third-party contributions
        ├── ada/                 # Ada bindings
        ├── blast/               # PKWare DCL decompressor
        ├── crc32vx/             # Vectorized CRC-32 (s390x)
        ├── delphi/              # Delphi bindings
        ├── dotzlib/             # .NET bindings
        ├── gcc_gvmat64/         # x86-64 assembly optimizations
        └── ...
```

---

## Ownership

All archived projects are owned by `@YongDo-Hyun` as defined in `ci/OWNERS`:

```
/archived/projt-launcher/                            @YongDo-Hyun
/archived/projt-minicraft-modpack/                   @YongDo-Hyun
/archived/projt-modpack/                             @YongDo-Hyun
/archived/ptlibzippy/                                @YongDo-Hyun
```

---

## Historical Context

### Timeline

The archived projects represent different phases of Project Tick's development:

1. **Early phase** (2024–2025): MiniCraft Modpack was created as a community modpack
   project with seasonal releases
2. **ProjT Modpack** (2025): A curated modpack distributed through the ProjT Launcher
3. **ProjT Launcher** (2025–2026): The main Minecraft launcher, forked from Prism Launcher,
   representing the most significant engineering investment in the archive
4. **PTlibzippy** (2025–2026): A zlib fork created to solve symbol conflicts in the
   launcher's build system

### Relationship to Current Projects

| Archived Project     | Successor/Replacement             |
|---------------------|-----------------------------------|
| ProjT Launcher      | MeshMC (`meshmc/`)                |
| ProjT Modpack       | No direct successor               |
| MiniCraft Modpack   | No direct successor               |
| PTlibzippy          | System zlib (no longer bundled)   |

---

## Policy

### Modifying Archived Code

Archived projects should generally not be modified. Exceptions:

- **License compliance**: Updating license headers or COPYING files
- **Security fixes**: Critical vulnerabilities in code that might be referenced externally
- **Documentation**: Fixing links, adding archival notes

### Removing Archived Projects

Archived projects should not be removed from the monorepo. They serve as:
- Historical reference for design decisions
- License compliance (preserving upstream attribution)
- Knowledge base for understanding the evolution of current projects

### Referencing Archived Code

When referencing data or patterns from archived projects in new code:
- Copy the relevant code rather than importing from `archived/`
- Document the source with a comment
- Ensure license compatibility

---

## Related Documentation

- [ProjT Launcher](projt-launcher.md) — Detailed launcher documentation
- [ProjT Modpack](projt-modpack.md) — Modpack project details
- [PTlibzippy](ptlibzippy.md) — Compression library documentation
