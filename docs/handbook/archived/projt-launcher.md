# ProjT Launcher

## Overview

ProjT Launcher was a structurally disciplined Minecraft launcher engineered for long-term
maintainability, architectural clarity, and controlled ecosystem evolution. It was a fork
of Prism Launcher (itself forked from PolyMC, which forked from MultiMC) that diverged
intentionally to prevent maintenance decay, dependency drift, and architectural erosion.

**Status**: Archived — superseded by MeshMC (`meshmc/`).

---

## Project Identity

| Property          | Value                                                   |
|-------------------|--------------------------------------------------------|
| **Name**          | ProjT Launcher                                         |
| **Location**      | `archived/projt-launcher/`                             |
| **Language**      | C++23 / Qt 6                                           |
| **Build System**  | CMake 3.25+                                            |
| **License**       | GPL-3.0-only                                           |
| **Copyright**     | 2026 Project Tick                                      |
| **Upstream**      | Prism Launcher → PolyMC → MultiMC                     |
| **Last Version**  | 0.0.5-1 (draft)                                        |
| **Website**       | https://projecttick.org/p/projt-launcher/              |
| **Releases**      | https://gitlab.com/Project-Tick/core/ProjT-Launcher/-/releases |

---

## Why ProjT Launcher Existed

The README states four key motivations:

1. **Long-term maintainability** — Explicit architectural constraints and review rules
   prevent uncontrolled technical debt
2. **Controlled third-party integration** — External dependencies are maintained as
   detached forks with documented patch and update policies
3. **Deterministic CI and builds** — Exact dependency versions and constrained build
   inputs enable reproducible builds across environments
4. **Structural clarity** — Enforced MVVM boundaries and clearly separated modules
   simplify review, refactoring, and long-term contribution

---

## Architecture

### Layered Model

The launcher followed a strict layered architecture documented in
`docs/architecture/OVERVIEW.md`:

```
┌─────────────────────────────────────────────────────────┐
│  Layer 1: UI + ViewModels (launcher/ui/, viewmodels/)    │
│  Qt Widgets screens, dialogs, widgets                    │
├─────────────────────────────────────────────────────────┤
│  Layer 2: Core/Domain (launcher/, minecraft/, java/)     │
│  Models, settings, instance management, launch logic     │
├─────────────────────────────────────────────────────────┤
│  Layer 3: Task System (launcher/tasks/)                  │
│  Long-running async work: downloads, extraction          │
├─────────────────────────────────────────────────────────┤
│  Layer 4: Networking (launcher/net/)                      │
│  HTTP requests, API adapters                             │
├─────────────────────────────────────────────────────────┤
│  Layer 5: Mod Platform Integrations (modplatform/)       │
│  Modrinth, CurseForge, ATLauncher, Technic, FTB         │
└─────────────────────────────────────────────────────────┘
```

### Module Boundaries

| Rule | Description |
|------|-------------|
| UI must not perform I/O | No file or network operations in the UI layer |
| Core/Tasks must not depend on Qt Widgets | Keeps the domain logic testable |
| ViewModels must be widget-free | Only expose data and actions |
| Use Task for anything > few milliseconds | Background jobs with progress reporting |
| Dependencies flow downward | `ui` → `core` → `data` (storage/net) |

### Directory Layout

```
ProjT-Launcher/
├── launcher/              # Main application
│   ├── ui/                # Qt Widgets
│   │   ├── pages/         # Main screens
│   │   ├── widgets/       # Reusable components
│   │   ├── dialogs/       # Modal windows
│   │   └── setupwizard/   # First-run wizard
│   ├── minecraft/         # Game logic
│   │   ├── auth/          # Account authentication (Microsoft)
│   │   ├── launch/        # Game process management
│   │   ├── mod/           # Mod loading and management
│   │   └── versions/      # Version parsing and resolution
│   ├── net/               # Networking layer
│   ├── tasks/             # Background job system
│   ├── java/              # Java runtime discovery and management
│   ├── modplatform/       # Mod platform APIs
│   ├── resources/         # Images, themes, assets
│   ├── icons/             # Application icons
│   └── translations/      # Internationalization files (.ts)
├── tests/                 # Unit tests
├── cmake/                 # CMake build modules
├── docs/                  # Documentation
├── website/               # Eleventy-based project website
├── bot/                   # Automation (Cloudflare Workers)
└── meta/                  # Metadata generator (Python)
```

---

## Build System

### CMake Configuration

The root `CMakeLists.txt` began with:

```cmake
cmake_minimum_required(VERSION 3.25)
project(Launcher)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
```

### Build Presets

```bash
cmake --preset [macos OR linux OR windows_msvc OR windows_mingw]
cmake --build --preset [macos OR linux OR windows_msvc OR windows_mingw] --config [Debug OR Release]
```

### Requirements

| Tool     | Version  |
|----------|----------|
| CMake    | 3.25+    |
| Qt       | 6.10.x   |
| Compiler | C++20/23 |

### Compiler Flags (MSVC)

```cmake
# Security and optimization flags:
"$<$<COMPILE_LANGUAGE:C,CXX>:/GS>"        # Buffer security checks
"$<$<CONFIG:Release>:/Gw;/Gy;/guard:cf>"  # Size optimization + control flow guard
"$<$<COMPILE_LANGUAGE:C,CXX>:/LTCG;/MANIFEST:NO;/STACK:8388608>"  # LTO, 8MB stack
```

The 8MB stack size was required because ATL's pack list needed 3-4 MiB as of the
time of development.

### Output Directory Macros

The build system used custom macros for managing output directories:

```cmake
macro(projt_push_output_dirs name)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${Launcher_OUTPUT_ROOT}/${name}/$<CONFIG>")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${Launcher_OUTPUT_ROOT}/${name}/$<CONFIG>")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${Launcher_OUTPUT_ROOT}/${name}/$<CONFIG>")
endmacro()

macro(projt_pop_output_dirs)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_PROJT_PREV_RUNTIME}")
    ...
endmacro()
```

Similar push/pop macros existed for:
- `projt_push_install_libdir` / `projt_pop_install_libdir`
- `projt_push_install_includedir` / `projt_pop_install_includedir`
- `projt_push_install_libexecdir` / `projt_pop_install_libexecdir`
- `projt_push_autogen_disabled` / `projt_pop_autogen_disabled`

These allowed different build components to use isolated output directories without
polluting the global CMake state.

### Linux Installation Paths

```cmake
set(Launcher_BUNDLED_LIBDIR "${CMAKE_INSTALL_LIBDIR}/projtlauncher")
set(Launcher_BUNDLED_INCLUDEDIR "include/projtlauncher")
set(Launcher_BUNDLED_LIBEXECDIR "libexec/projtlauncher")
```

### Qt Deprecation Policy

```cmake
add_compile_definitions(QT_WARN_DEPRECATED_UP_TO=0x060400)
add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x060400)
```

This configured Qt to warn about APIs deprecated before Qt 6.4.0 and hard-disable
them at compile time.

---

## Nix Build

The `default.nix` used `flake-compat` to provide a traditional Nix interface:

```nix
(import (fetchTarball {
  url = "https://github.com/edolstra/flake-compat/archive/ff81ac966bb2cae68946d5ed5fc4994f96d0ffec.tar.gz";
  sha256 = "sha256-NeCCThCEP3eCl2l/+27kNNK7QrwZB1IJCrXfrbv5oqU=";
}) { src = ./.; }).defaultNix
```

Quick build:

```bash
nix build .#projtlauncher
```

---

## Container Build

The `Containerfile` defined a Debian-based build environment:

```dockerfile
ARG DEBIAN_VERSION=stable-slim
FROM docker.io/library/debian:${DEBIAN_VERSION}

ARG QT_VERSION=6.10.2

# Compilers: clang, lld, llvm, temurin-17-jdk
# Build system: cmake, ninja-build, extra-cmake-modules, pkg-config
# Dependencies: cmark, gamemode-dev, libarchive-dev, libcmark-dev,
#               libgl1-mesa-dev, libqrencode-dev, libtomlplusplus-dev,
#               scdoc, zlib1g-dev
# Tooling: clang-format, clang-tidy, git

ENV CMAKE_LINKER_TYPE=lld
```

Qt was installed via `aqtinstall`:

```dockerfile
RUN pip3 install --break-system-packages aqtinstall
RUN aqt install-qt ...
```

---

## Detached Fork Libraries

The launcher maintained its own forks of several upstream libraries:

| Library        | Directory      | Purpose                |
|---------------|----------------|------------------------|
| PTlibzippy    | `ptlibzippy/`  | Compression (zlib fork)|
| bzip2         | `bzip2/`       | Compression            |
| quazip        | `quazip/`      | ZIP handling           |
| cmark         | `cmark/`       | Markdown parsing       |
| tomlplusplus  | `tomlplusplus/`| TOML parsing           |
| libqrencode   | `libqrencode/` | QR code generation     |
| libnbtplusplus| `libnbtplusplus/` | NBT format (Minecraft)|
| gamemode      | `gamemode/`    | Linux GameMode support |

These were maintained with documented patch and update policies to prevent
dependency drift while staying reasonably current with upstream.

### Vendored Libraries

| Library    | Directory      | Purpose            |
|-----------|----------------|--------------------|
| LocalPeer | `LocalPeer/`   | Single instance    |
| murmur2   | `murmur2/`     | Hash functions     |
| qdcss     | `qdcss/`       | Dark CSS           |
| rainbow   | `rainbow/`     | Terminal colors    |
| systeminfo| `systeminfo/`  | System information |

---

## Features at Time of Archival

### Changelog (v0.0.5-1 Draft)

**Highlights from the last release cycle:**

- Improved Fabric/Quilt component version resolution with better Minecraft-version alignment
- Added Launcher Hub support (web-based dashboard)
- Strengthened version comparison logic, especially for release-candidate handling
- Added Modrinth collection import for existing instances
- Switched Linux Launcher Hub backend from QtWebEngine to CEF
- Added native cockpit dashboard for Launcher Hub

**Platform support:**

| Platform   | Backend              | Packaging                    |
|-----------|----------------------|------------------------------|
| Linux     | CEF-based Hub        | DEB, RPM, AppImage, Flatpak |
| macOS     | Native WebView       | App Bundle                  |
| Windows   | Native WebView       | MSI, Portable               |

---

## CI Infrastructure (Pre-Monorepo)

The launcher had its own CI infrastructure in `ci/`, which was the predecessor
to the current monorepo CI system. It included:

- `ci/default.nix` — Nix CI entry point
- `ci/pinned.json` — Pinned dependencies
- `ci/supportedBranches.js` — Branch classification
- `ci/github-script/` — GitHub Actions helpers
- `ci/eval/` — Nix evaluation infrastructure
  - `attrpaths.nix` — Attribute path enumeration
  - `chunk.nix` — Evaluation chunking
  - `diff.nix` — Evaluation diffing
  - `outpaths.nix` — Output path computation
  - `compare/` — Statistics comparison
- `ci/nixpkgs-vet.nix` / `ci/nixpkgs-vet.sh` — Nixpkgs vetting
- `ci/parse.nix` — CI configuration parsing
- `ci/supportedSystems.json` — Supported target systems
- `ci/supportedVersions.nix` — Supported version matrix

Some of these patterns were carried forward into the monorepo CI system.

---

## Documentation Structure

The launcher had extensive documentation:

```
docs/
├── APPLE_SILICON_RATIONALE.md
├── BUILD_SYSTEM.md
├── FUZZING.md
├── README.md
├── architecture/
│   └── OVERVIEW.md
├── contributing/
│   ├── ARCHITECTURE.md
│   ├── CODE_STYLE.md
│   ├── GETTING_STARTED.md
│   ├── LAUNCHER_TEST_MATRIX.md
│   ├── PROJECT_STRUCTURE.md
│   ├── README.md
│   ├── TESTING.md
│   └── WORKFLOW.md
└── handbook/
    ├── README.md
    ├── bot.md, bzip2.md, cmark.md, ...
    ├── help-pages/
    │   ├── apis.md, custom-commands.md, ...
    │   └── environment-variables.md
    └── wiki/
        ├── development/
        │   ├── instructions/
        │   │   ├── linux.md, macos.md, windows.md
        │   └── translating.md
        ├── getting-started/
        │   ├── installing-projtlauncher.md
        │   ├── installing-java.md
        │   ├── create-instance.md
        │   └── download-modpacks.md
        └── help-pages/
            └── ... (mirrors of handbook help-pages)
```

---

## Maintainership

```
[Mehmet Samet Duman]
GitHub: @YongDo-Hyun
Email: yongdohyun@mail.projecttick.org
Paths: **
```

The project was maintained by a single maintainer with full ownership of all paths.

---

## License

The launcher carried a multi-layer license history:

```
ProjT Launcher - Minecraft Launcher
Copyright (C) 2026 Project Tick
License: GPL-3.0-only

Incorporates work from:
├── Prism Launcher (Copyright 2022-2025 Prism Launcher Contributors, GPL-3.0)
│   └── Incorporates:
│       └── MultiMC (Copyright 2013-2021 MultiMC Contributors, Apache-2.0)
└── PolyMC (Copyright 2021-2022 PolyMC Contributors, GPL-3.0)
```

The logo carried a separate license:
- Original: Prism Launcher Logo © Prism Launcher Contributors (CC BY-SA 4.0)
- Modified: ProjT Launcher Logo © 2026 Project Tick (CC BY-SA 4.0)

---

## Why It Was Archived

ProjT Launcher was archived when MeshMC (`meshmc/`) became the primary launcher
in the Project Tick monorepo. MeshMC continued the development trajectory with:
- Updated architecture decisions
- Continued the same mod platform integrations
- Maintained the same CMake/Qt/Nix build infrastructure
- Carried forward the detached fork library approach

The launcher code remains in `archived/` as a reference for:
- Design patterns (layered architecture, task system)
- Build system techniques (CMake push/pop macros, vcpkg integration)
- CI patterns (GitHub script infrastructure)
- License compliance (preserving upstream attribution chains)

---

## Building (for Reference)

If someone needs to build the archived launcher for historical purposes:

```bash
cd archived/projt-launcher/
git submodule update --init --recursive

# Linux:
cmake --preset linux
cmake --build --preset linux --config Release

# macOS:
cmake --preset macos
cmake --build --preset macos --config Release

# Windows (MSVC):
cmake --preset windows_msvc
cmake --build --preset windows_msvc --config Release
```

Note: Build success is not guaranteed since the archived code is not maintained
and dependencies may have changed.
