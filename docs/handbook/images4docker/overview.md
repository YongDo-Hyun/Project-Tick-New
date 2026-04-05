# images4docker — Overview

## What Is images4docker?

**images4docker** (formally titled "Project Tick Infra Image Repack") is a repository
that maintains **40 separate Dockerfiles**, each pinned to a distinct upstream Linux
distribution base image.  Every Dockerfile follows an identical, parameterised
template that:

1. Pulls a fixed upstream base (`FROM <distro>:<tag>`).
2. Accepts four build-time arguments (`PACKAGES`, `CUSTOM_INSTALL`, `UPDATE_CMD`,
   `CLEAN_CMD`) so the CI workflow can inject the exact package list at build time.
3. Installs the requested packages using the distro's native package manager.
4. Runs a mandatory **Qt6 toolchain verification** gate — the build *fails* if
   `qmake6`, `qmake-qt6`, or `qtpaths6` cannot be found in any of the standard
   Qt6 binary paths.
5. Cleans caches and sets `/bin/sh` as the default `CMD`.

The resulting images are pushed to the GitHub Container Registry at:

```
ghcr.io/project-tick-infra/images/<target_name>:<target_tag>
```

These images serve as the **standardised build and test environments** for all
Project Tick CI/CD pipelines.  Rather than installing dependencies inside every
CI job, the project pre-bakes them into reusable Docker images that are rebuilt
daily and on every push to `main`.

---

## Why Does images4docker Exist?

CI/CD jobs that compile C/C++ projects with Qt6 dependencies need a consistent
set of development libraries, headers, and toolchains.  Doing `apt-get install`
or `dnf install` at the start of every job is:

- **Slow** — package downloads and dependency resolution add minutes.
- **Fragile** — upstream mirrors change, packages get renamed, GPG keys rotate.
- **Non-reproducible** — two jobs that start seconds apart can get different
  package versions.

images4docker solves this by providing **pre-built, version-pinned, Qt6-verified
Docker images** for every distribution the project needs to support.

---

## Repository Location

Within the Project Tick monorepo:

```
Project-Tick/
└── images4docker/
    ├── .gitattributes
    ├── .gitignore
    ├── LICENSE              (GPL-3.0-or-later)
    ├── LICENSES/
    │   └── GPL-3.0-or-later.txt
    ├── README.md
    └── dockerfiles/
        ├── alma-9.Dockerfile
        ├── alma-10.Dockerfile
        ├── alpine-319.Dockerfile
        ├── alpine-320.Dockerfile
        ├── alpine-321.Dockerfile
        ├── alpine-322.Dockerfile
        ├── alpine-latest.Dockerfile
        ├── amazonlinux-2.Dockerfile
        ├── amazonlinux-2023.Dockerfile
        ├── arch-latest.Dockerfile
        ├── centos-stream9.Dockerfile
        ├── centos-stream10.Dockerfile
        ├── debian-bookworm.Dockerfile
        ├── debian-bookworm-slim.Dockerfile
        ├── debian-bullseye.Dockerfile
        ├── debian-bullseye-slim.Dockerfile
        ├── debian-stable-slim.Dockerfile
        ├── debian-trixie-slim.Dockerfile
        ├── devuan-chimaera.Dockerfile
        ├── devuan-daedalus.Dockerfile
        ├── fedora-40.Dockerfile
        ├── fedora-41.Dockerfile
        ├── fedora-42.Dockerfile
        ├── fedora-latest.Dockerfile
        ├── gentoo-stage3.Dockerfile
        ├── kali-rolling.Dockerfile
        ├── nix-latest.Dockerfile
        ├── opensuse-leap-155.Dockerfile
        ├── opensuse-leap-156.Dockerfile
        ├── opensuse-tumbleweed.Dockerfile
        ├── oraclelinux-8.Dockerfile
        ├── oraclelinux-9.Dockerfile
        ├── oraclelinux-10.Dockerfile
        ├── rocky-9.Dockerfile
        ├── rocky-10.Dockerfile
        ├── ubuntu-2004.Dockerfile
        ├── ubuntu-2204.Dockerfile
        ├── ubuntu-2404.Dockerfile
        ├── ubuntu-latest.Dockerfile
        └── void-latest.Dockerfile
```

---

## Complete Image Inventory

The 40 Dockerfiles map to the following image targets.  They are grouped below by
distribution family.

### RHEL / Enterprise Linux Family

| Dockerfile                   | FROM base image                          | Package manager | Notes                                   |
|------------------------------|------------------------------------------|-----------------|-----------------------------------------|
| `alma-9.Dockerfile`         | `almalinux:9`                            | `dnf`           | RHEL 9 binary-compatible rebuild        |
| `alma-10.Dockerfile`        | `almalinux:10`                           | `dnf`           | RHEL 10 binary-compatible rebuild       |
| `centos-stream9.Dockerfile` | `quay.io/centos/centos:stream9`          | `dnf`           | CentOS Stream 9, upstream of RHEL 9     |
| `centos-stream10.Dockerfile`| `quay.io/centos/centos:stream10`         | `dnf`           | CentOS Stream 10, upstream of RHEL 10   |
| `oraclelinux-8.Dockerfile`  | `oraclelinux:8`                          | `dnf`           | Oracle Linux 8 (RHEL 8 compatible)      |
| `oraclelinux-9.Dockerfile`  | `oraclelinux:9`                          | `dnf`           | Oracle Linux 9 (RHEL 9 compatible)      |
| `oraclelinux-10.Dockerfile` | `oraclelinux:10`                         | `dnf`           | Oracle Linux 10 (RHEL 10 compatible)    |
| `rocky-9.Dockerfile`        | `rockylinux/rockylinux:9`                | `dnf`           | Rocky Linux 9 (RHEL 9 compatible)       |
| `rocky-10.Dockerfile`       | `rockylinux/rockylinux:10`               | `dnf`           | Rocky Linux 10 (RHEL 10 compatible)     |

### Amazon Linux

| Dockerfile                      | FROM base image        | Package manager | Notes                          |
|---------------------------------|------------------------|-----------------|---------------------------------|
| `amazonlinux-2.Dockerfile`     | `amazonlinux:2`        | `yum`           | Amazon Linux 2 (legacy)        |
| `amazonlinux-2023.Dockerfile`  | `amazonlinux:2023`     | `dnf`           | Amazon Linux 2023 (current)    |

### Fedora

| Dockerfile                  | FROM base image    | Package manager | Notes                     |
|-----------------------------|--------------------|-----------------|---------------------------|
| `fedora-40.Dockerfile`     | `fedora:40`        | `dnf`           | Fedora 40                 |
| `fedora-41.Dockerfile`     | `fedora:41`        | `dnf`           | Fedora 41                 |
| `fedora-42.Dockerfile`     | `fedora:42`        | `dnf`           | Fedora 42                 |
| `fedora-latest.Dockerfile` | `fedora:latest`    | `dnf`           | Fedora rolling latest     |

### Debian

| Dockerfile                         | FROM base image           | Package manager | Notes                     |
|-------------------------------------|---------------------------|-----------------|---------------------------|
| `debian-bookworm.Dockerfile`       | `debian:bookworm`         | `apt`           | Debian 12 full            |
| `debian-bookworm-slim.Dockerfile`  | `debian:bookworm-slim`    | `apt`           | Debian 12 minimal         |
| `debian-bullseye.Dockerfile`       | `debian:bullseye`         | `apt`           | Debian 11 full            |
| `debian-bullseye-slim.Dockerfile`  | `debian:bullseye-slim`    | `apt`           | Debian 11 minimal         |
| `debian-stable-slim.Dockerfile`    | `debian:stable-slim`      | `apt`           | Current stable, minimal   |
| `debian-trixie-slim.Dockerfile`    | `debian:trixie-slim`      | `apt`           | Debian 13 (testing) slim  |

### Devuan

| Dockerfile                      | FROM base image             | Package manager | Notes                    |
|----------------------------------|-----------------------------|-----------------|--------------------------|
| `devuan-chimaera.Dockerfile`   | `devuan/devuan:chimaera`    | `apt`           | Devuan 4 (systemd-free)  |
| `devuan-daedalus.Dockerfile`   | `devuan/devuan:daedalus`    | `apt`           | Devuan 5 (systemd-free)  |

### Ubuntu

| Dockerfile                   | FROM base image    | Package manager | Notes                  |
|-------------------------------|--------------------|-----------------|------------------------|
| `ubuntu-2004.Dockerfile`    | `ubuntu:20.04`     | `apt`           | Ubuntu 20.04 LTS       |
| `ubuntu-2204.Dockerfile`    | `ubuntu:22.04`     | `apt`           | Ubuntu 22.04 LTS       |
| `ubuntu-2404.Dockerfile`    | `ubuntu:24.04`     | `apt`           | Ubuntu 24.04 LTS       |
| `ubuntu-latest.Dockerfile`  | `ubuntu:latest`    | `apt`           | Ubuntu rolling latest   |

### Kali Linux

| Dockerfile                   | FROM base image                     | Package manager | Notes         |
|-------------------------------|-------------------------------------|-----------------|---------------|
| `kali-rolling.Dockerfile`   | `kalilinux/kali-rolling:latest`     | `apt`           | Kali rolling  |

### Alpine Linux

| Dockerfile                    | FROM base image    | Package manager | Notes           |
|--------------------------------|--------------------|-----------------|-----------------|
| `alpine-319.Dockerfile`      | `alpine:3.19`      | `apk`           | Alpine 3.19     |
| `alpine-320.Dockerfile`      | `alpine:3.20`      | `apk`           | Alpine 3.20     |
| `alpine-321.Dockerfile`      | `alpine:3.21`      | `apk`           | Alpine 3.21     |
| `alpine-322.Dockerfile`      | `alpine:3.22`      | `apk`           | Alpine 3.22     |
| `alpine-latest.Dockerfile`   | `alpine:latest`    | `apk`           | Alpine edge     |

### openSUSE

| Dockerfile                          | FROM base image                   | Package manager | Notes                |
|--------------------------------------|-----------------------------------|-----------------|-----------------------|
| `opensuse-leap-155.Dockerfile`      | `opensuse/leap:15.5`              | `zypper`        | Leap 15.5            |
| `opensuse-leap-156.Dockerfile`      | `opensuse/leap:15.6`              | `zypper`        | Leap 15.6            |
| `opensuse-tumbleweed.Dockerfile`    | `opensuse/tumbleweed:latest`      | `zypper`        | Tumbleweed rolling   |

### Arch Linux

| Dockerfile                  | FROM base image        | Package manager | Notes              |
|------------------------------|------------------------|-----------------|--------------------|
| `arch-latest.Dockerfile`   | `archlinux:latest`     | `pacman`        | Arch rolling       |

### Gentoo

| Dockerfile                   | FROM base image            | Package manager | Notes              |
|-------------------------------|----------------------------|-----------------|--------------------|
| `gentoo-stage3.Dockerfile`  | `gentoo/stage3:latest`     | `emerge`        | Gentoo Stage 3     |

### NixOS

| Dockerfile                 | FROM base image       | Package manager | Notes          |
|-----------------------------|-----------------------|-----------------|----------------|
| `nix-latest.Dockerfile`   | `nixos/nix:latest`    | `nix-env`       | NixOS/Nix      |

### Void Linux

| Dockerfile                  | FROM base image                 | Package manager | Notes          |
|------------------------------|---------------------------------|-----------------|----------------|
| `void-latest.Dockerfile`   | `voidlinux/voidlinux:latest`    | `xbps`          | Void rolling   |

---

## Distribution Coverage Summary

| Package Manager Family | Count | Distributions                                                                  |
|------------------------|-------|--------------------------------------------------------------------------------|
| `dnf`                  | 14    | AlmaLinux, CentOS Stream, Oracle Linux, Rocky Linux, Fedora, Amazon Linux 2023 |
| `apt`                  | 14    | Debian, Ubuntu, Devuan, Kali                                                   |
| `apk`                  | 5     | Alpine Linux                                                                   |
| `zypper`               | 3     | openSUSE Leap, openSUSE Tumbleweed                                            |
| `pacman`               | 1     | Arch Linux                                                                     |
| `yum`                  | 1     | Amazon Linux 2                                                                 |
| `emerge`               | 1     | Gentoo                                                                         |
| `nix-env`              | 1     | NixOS/Nix                                                                      |
| `xbps`                 | 1     | Void Linux                                                                     |
| **Total**              | **40**|                                                                                |

---

## Workflow Automation

The images are built by a GitHub Actions workflow named **"Repack Existing Images"**
(`.github/workflows/build.yml`).  It triggers on:

- **Push to `main`** — when any `Dockerfile`, the workflow YAML, or `README.md` changes.
- **Daily schedule** — cron at `03:17 UTC` every day.

On each run the workflow builds and pushes the **Qt6-compatible set** (currently
35 of the 40 targets — some older bases are excluded because they cannot provide
Qt6 reliably).

---

## Licensing

images4docker is licensed under the **GNU General Public License v3.0 or later**
(GPL-3.0-or-later).  The full license text is stored in:

- `LICENSE` (top-level)
- `LICENSES/GPL-3.0-or-later.txt`

---

## Key Design Decisions

1. **One Dockerfile per base image** — no multi-stage, no shared `FROM`.  This
   keeps each image independent and debuggable.

2. **Build-arg-driven customisation** — the Dockerfile itself contains only the
   package-manager dispatch and the Qt6 check.  All concrete package lists are
   injected via `--build-arg PACKAGES=...` by the CI workflow.

3. **Qt6 or bust** — there is no Qt5 fallback.  If a distribution does not
   package Qt6 tools, the build intentionally fails.

4. **Syntax directive** — every Dockerfile starts with `# syntax=docker/dockerfile:1.7`
   to opt into BuildKit features.

5. **LF line endings enforced** — `.gitattributes` sets `* text=auto eol=lf` and
   marks `*.Dockerfile` as text to prevent CRLF issues on Windows.

6. **Minimal `.gitignore`** — only `*.log`, `*.tmp`, and `.env` are ignored.

---

## Quick Reference

| Item                    | Value                                                  |
|-------------------------|---------------------------------------------------------|
| Total Dockerfiles       | 40                                                     |
| Active CI matrix        | ~35 (Qt6-capable targets)                              |
| Registry                | `ghcr.io/project-tick-infra/images/`                   |
| Build trigger (push)    | Changes to `dockerfiles/`, workflow, `README.md`       |
| Build trigger (cron)    | Daily at `03:17 UTC`                                   |
| Qt6 requirement         | Mandatory — build fails without it                     |
| License                 | GPL-3.0-or-later                                       |
| Dockerfile syntax       | `docker/dockerfile:1.7` (BuildKit)                     |
| Default CMD             | `/bin/sh`                                              |

---

## Related Documentation

- [Architecture](architecture.md) — directory layout and Dockerfile structure
- [Base Images](base-images.md) — per-image deep dive
- [Qt6 Verification](qt6-verification.md) — how the Qt6 gate works
- [CI/CD Integration](ci-cd-integration.md) — workflow and registry details
- [Creating New Images](creating-new-images.md) — how to add a new distribution
- [Troubleshooting](troubleshooting.md) — common issues and debugging
