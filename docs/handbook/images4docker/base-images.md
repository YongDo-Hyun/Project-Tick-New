# images4docker — Base Images

## Overview

This document provides an exhaustive reference for every base image used by
images4docker.  Each entry covers the upstream `FROM` reference, the package
manager used, the install and cleanup commands, the Qt6 search paths, and
notes about distribution-specific behaviour.

All 40 Dockerfiles share the same template structure (see
[Architecture](architecture.md)).  The differences are:

1. The `FROM` image reference.
2. The native package-manager command.
3. The default cache-cleanup command.
4. The Qt6 binary search `PATH` extensions.

---

## RHEL / Enterprise Linux Family

### AlmaLinux 9

**File:** `dockerfiles/alma-9.Dockerfile`
**FROM:** `almalinux:9`
**Package manager:** `dnf`

```dockerfile
# syntax=docker/dockerfile:1.7
FROM almalinux:9

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux; \
  ... \
  dnf install -y ${PACKAGES}; \
  ... \
  dnf clean all || true; \
  export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin"; \
  <qt6 verification>

CMD ["/bin/sh"]
```

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Binary-compatible rebuild of RHEL 9.
- Qt6 packages typically available via EPEL or CRB repositories (enabled via `CUSTOM_INSTALL`).
- CRB (CodeReady Builder) / PowerTools sometimes needs explicit enablement: `dnf config-manager --enable crb`.

---

### AlmaLinux 10

**File:** `dockerfiles/alma-10.Dockerfile`
**FROM:** `almalinux:10`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- RHEL 10 compatible rebuild.
- Qt6 packages expected to be more widely available in RHEL 10 repositories compared to RHEL 9.

---

### CentOS Stream 9

**File:** `dockerfiles/centos-stream9.Dockerfile`
**FROM:** `quay.io/centos/centos:stream9`
**Package manager:** `dnf`

```dockerfile
FROM quay.io/centos/centos:stream9
```

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Official CentOS Stream images are hosted on **Quay.io**, not Docker Hub.
- CentOS Stream 9 is the upstream development branch for RHEL 9.
- The `quay.io/centos/centos` namespace replaced the former `centos` Docker Hub image.

---

### CentOS Stream 10

**File:** `dockerfiles/centos-stream10.Dockerfile`
**FROM:** `quay.io/centos/centos:stream10`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Upstream development branch for RHEL 10.
- Hosted on Quay.io at `quay.io/centos/centos:stream10`.

---

### Oracle Linux 8

**File:** `dockerfiles/oraclelinux-8.Dockerfile`
**FROM:** `oraclelinux:8`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, **`/usr/libexec/qt6`**, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Qt6 extra checks:** Also checks `/usr/libexec/qt6/qmake` and `/usr/libexec/qt6/qtpaths`
**Notes:**
- RHEL 8 compatible.  Oracle Linux 8 may have limited Qt6 availability.
- Has the extended Qt6 path including `/usr/libexec/qt6` where Oracle may place Qt6 binaries.
- May require `CUSTOM_INSTALL` to enable additional repositories for Qt6.

---

### Oracle Linux 9

**File:** `dockerfiles/oraclelinux-9.Dockerfile`
**FROM:** `oraclelinux:9`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, **`/usr/libexec/qt6`**, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Qt6 extra checks:** Also checks `/usr/libexec/qt6/qmake` and `/usr/libexec/qt6/qtpaths`

---

### Oracle Linux 10

**File:** `dockerfiles/oraclelinux-10.Dockerfile`
**FROM:** `oraclelinux:10`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, **`/usr/libexec/qt6`**, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Qt6 extra checks:** Also checks `/usr/libexec/qt6/qmake` and `/usr/libexec/qt6/qtpaths`

---

### Rocky Linux 9

**File:** `dockerfiles/rocky-9.Dockerfile`
**FROM:** `rockylinux/rockylinux:9`
**Package manager:** `dnf`

```dockerfile
FROM rockylinux/rockylinux:9
```

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Rocky Linux images are under the `rockylinux/rockylinux` namespace on Docker Hub.
- RHEL 9 binary-compatible community rebuild (successor to CentOS Linux).

---

### Rocky Linux 10

**File:** `dockerfiles/rocky-10.Dockerfile`
**FROM:** `rockylinux/rockylinux:10`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`

---

## Amazon Linux

### Amazon Linux 2

**File:** `dockerfiles/amazonlinux-2.Dockerfile`
**FROM:** `amazonlinux:2`
**Package manager:** `yum`

```dockerfile
FROM amazonlinux:2

RUN set -eux; \
  ... \
  yum install -y ${PACKAGES}; \
  ... \
  yum clean all || true; \
  ...
```

**Install command:** `yum install -y ${PACKAGES}`
**Cleanup command:** `yum clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- This is the **only** image that uses `yum` instead of `dnf`.
- Amazon Linux 2 is based on RHEL 7 / CentOS 7 era packages.
- Qt6 availability is **very limited** on Amazon Linux 2.  This image is likely
  excluded from the active CI matrix (the README states ~35 of 40 are active).
- May require a `CUSTOM_INSTALL` command to build Qt6 from source or use a
  third-party repository.

---

### Amazon Linux 2023

**File:** `dockerfiles/amazonlinux-2023.Dockerfile`
**FROM:** `amazonlinux:2023`
**Package manager:** `dnf`

```dockerfile
FROM amazonlinux:2023

RUN set -eux; \
  ... \
  dnf install -y ${PACKAGES}; \
  ... \
  dnf clean all || true; \
  export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/usr/libexec/qt6:/opt/qt6/bin:/root/.nix-profile/bin"; \
  ...
```

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, **`/usr/libexec/qt6`**, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Qt6 extra checks:** Also checks `/usr/libexec/qt6/qmake` and `/usr/libexec/qt6/qtpaths`
**Notes:**
- Amazon Linux 2023 uses `dnf` (not `yum`), aligning with modern Fedora/RHEL.
- Has the extended `/usr/libexec/qt6` path, suggesting Qt6 packages may install to libexec on AL2023.

---

## Fedora

### Fedora 40

**File:** `dockerfiles/fedora-40.Dockerfile`
**FROM:** `fedora:40`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Fedora has excellent Qt6 support.  Packages like `qt6-qtbase-devel` are available
  directly in the default repositories.

---

### Fedora 41

**File:** `dockerfiles/fedora-41.Dockerfile`
**FROM:** `fedora:41`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`

---

### Fedora 42

**File:** `dockerfiles/fedora-42.Dockerfile`
**FROM:** `fedora:42`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`

---

### Fedora Latest

**File:** `dockerfiles/fedora-latest.Dockerfile`
**FROM:** `fedora:latest`
**Package manager:** `dnf`

**Install command:** `dnf install -y ${PACKAGES}`
**Cleanup command:** `dnf clean all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Rolling tag that always points to the newest stable Fedora release.

---

## Debian Family

### Debian Bookworm (12)

**File:** `dockerfiles/debian-bookworm.Dockerfile`
**FROM:** `debian:bookworm`
**Package manager:** `apt`

```dockerfile
FROM debian:bookworm

RUN set -eux; \
  ... \
  apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}; \
  ... \
  rm -rf /var/lib/apt/lists/*; \
  ...
```

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Debian 12 "Bookworm" is the current stable release.
- Full variant — includes documentation, man pages, standard utilities.
- Qt6 packages are available as `qt6-base-dev`, `qmake6`, etc.
- `--no-install-recommends` is critical for keeping image size down.

---

### Debian Bookworm Slim

**File:** `dockerfiles/debian-bookworm-slim.Dockerfile`
**FROM:** `debian:bookworm-slim`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Minimal Debian 12 — roughly half the size of the full variant.
- No man pages, no documentation packages.
- Preferred for CI where download speed matters.

---

### Debian Bullseye (11)

**File:** `dockerfiles/debian-bullseye.Dockerfile`
**FROM:** `debian:bullseye`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Debian 11 "Bullseye" (old stable).
- Qt6 availability is **limited** — Bullseye shipped with Qt 5.15 in main.
  Qt6 may require backports or `CUSTOM_INSTALL`.
- Likely excluded from active CI matrix due to unreliable Qt6.

---

### Debian Bullseye Slim

**File:** `dockerfiles/debian-bullseye-slim.Dockerfile`
**FROM:** `debian:bullseye-slim`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Minimal variant of Debian 11.
- Same Qt6 limitations as the full Bullseye variant.

---

### Debian Stable Slim

**File:** `dockerfiles/debian-stable-slim.Dockerfile`
**FROM:** `debian:stable-slim`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Rolling tag pointing to current Debian stable (currently Bookworm).
- Automatically upgrades when a new Debian stable is released.

---

### Debian Trixie Slim

**File:** `dockerfiles/debian-trixie-slim.Dockerfile`
**FROM:** `debian:trixie-slim`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Debian 13 "Trixie" (testing).
- Provides the latest packages, including recent Qt6 versions.
- Good for catching regressions early with newer toolchains.

---

## Devuan

### Devuan Chimaera

**File:** `dockerfiles/devuan-chimaera.Dockerfile`
**FROM:** `devuan/devuan:chimaera`
**Package manager:** `apt`

```dockerfile
FROM devuan/devuan:chimaera
```

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Devuan 4 "Chimaera" — based on Debian Bullseye but **without systemd**.
- Uses `sysvinit` or OpenRC as init system.
- Images are under `devuan/devuan` namespace on Docker Hub.
- Qt6 availability mirrors Debian Bullseye (limited).

---

### Devuan Daedalus

**File:** `dockerfiles/devuan-daedalus.Dockerfile`
**FROM:** `devuan/devuan:daedalus`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Devuan 5 "Daedalus" — based on Debian Bookworm without systemd.
- Qt6 availability mirrors Debian Bookworm (good).

---

## Ubuntu

### Ubuntu 20.04 LTS (Focal Fossa)

**File:** `dockerfiles/ubuntu-2004.Dockerfile`
**FROM:** `ubuntu:20.04`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Ubuntu 20.04 LTS does **not** ship Qt6 in its default repositories.
- Qt6 requires PPAs, the Qt online installer, or building from source.
- Likely excluded from the active CI matrix.

---

### Ubuntu 22.04 LTS (Jammy Jellyfish)

**File:** `dockerfiles/ubuntu-2204.Dockerfile`
**FROM:** `ubuntu:22.04`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Ubuntu 22.04 ships Qt 6.2 LTS in the `universe` repository.
- Packages: `qt6-base-dev`, `qmake6`, `qt6-tools-dev`, etc.

---

### Ubuntu 24.04 LTS (Noble Numbat)

**File:** `dockerfiles/ubuntu-2404.Dockerfile`
**FROM:** `ubuntu:24.04`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Ubuntu 24.04 ships Qt 6.4+ in the default repositories.
- Full Qt6 development support out of the box.

---

### Ubuntu Latest

**File:** `dockerfiles/ubuntu-latest.Dockerfile`
**FROM:** `ubuntu:latest`
**Package manager:** `apt`

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Rolling tag pointing to the latest Ubuntu LTS release.

---

## Kali Linux

### Kali Rolling

**File:** `dockerfiles/kali-rolling.Dockerfile`
**FROM:** `kalilinux/kali-rolling:latest`
**Package manager:** `apt`

```dockerfile
FROM kalilinux/kali-rolling:latest
```

**Install command:** `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`
**Cleanup command:** `rm -rf /var/lib/apt/lists/*`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Kali is Debian-based, so it uses the same `apt` install pattern.
- Images are under `kalilinux/kali-rolling` on Docker Hub.
- Kali's rolling release usually has recent Qt6 packages.

---

## Alpine Linux

### Alpine 3.19

**File:** `dockerfiles/alpine-319.Dockerfile`
**FROM:** `alpine:3.19`
**Package manager:** `apk`

```dockerfile
FROM alpine:3.19

RUN set -eux; \
  ... \
  apk add --no-cache ${PACKAGES}; \
  ... \
  true; \
  ...
```

**Install command:** `apk add --no-cache ${PACKAGES}`
**Cleanup command:** `true` (no-op — `--no-cache` handles cleanup)
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Alpine uses `musl libc`, not `glibc`.
- Qt6 packages: `qt6-qtbase-dev`, `qt6-qttools-dev`, etc.
- No `libsystemd-dev` equivalent (Alpine does not use systemd).
- Very small base images (~7 MB compressed).

---

### Alpine 3.20

**File:** `dockerfiles/alpine-320.Dockerfile`
**FROM:** `alpine:3.20`
**Package manager:** `apk`

**Install command:** `apk add --no-cache ${PACKAGES}`
**Cleanup command:** `true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`

---

### Alpine 3.21

**File:** `dockerfiles/alpine-321.Dockerfile`
**FROM:** `alpine:3.21`
**Package manager:** `apk`

**Install command:** `apk add --no-cache ${PACKAGES}`
**Cleanup command:** `true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`

---

### Alpine 3.22

**File:** `dockerfiles/alpine-322.Dockerfile`
**FROM:** `alpine:3.22`
**Package manager:** `apk`

**Install command:** `apk add --no-cache ${PACKAGES}`
**Cleanup command:** `true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`

---

### Alpine Latest

**File:** `dockerfiles/alpine-latest.Dockerfile`
**FROM:** `alpine:latest`
**Package manager:** `apk`

**Install command:** `apk add --no-cache ${PACKAGES}`
**Cleanup command:** `true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Rolling tag pointing to the latest Alpine stable release.

---

## openSUSE

### openSUSE Leap 15.5

**File:** `dockerfiles/opensuse-leap-155.Dockerfile`
**FROM:** `opensuse/leap:15.5`
**Package manager:** `zypper`

```dockerfile
FROM opensuse/leap:15.5

RUN set -eux; \
  ... \
  zypper --non-interactive refresh; \
  zypper --non-interactive install --no-recommends ${PACKAGES}; \
  ... \
  zypper clean --all || true; \
  ...
```

**Install command:** `zypper --non-interactive refresh; zypper --non-interactive install --no-recommends ${PACKAGES}`
**Cleanup command:** `zypper clean --all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- openSUSE Leap 15.5 — enterprise-grade stability.
- Qt6 availability depends on the OBS (Open Build Service) repositories.
- `--non-interactive` prevents zypper from blocking on prompts.
- `--no-recommends` skips recommended packages (equivalent to `--no-install-recommends` in apt).

---

### openSUSE Leap 15.6

**File:** `dockerfiles/opensuse-leap-156.Dockerfile`
**FROM:** `opensuse/leap:15.6`
**Package manager:** `zypper`

**Install command:** `zypper --non-interactive refresh; zypper --non-interactive install --no-recommends ${PACKAGES}`
**Cleanup command:** `zypper clean --all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`

---

### openSUSE Tumbleweed

**File:** `dockerfiles/opensuse-tumbleweed.Dockerfile`
**FROM:** `opensuse/tumbleweed:latest`
**Package manager:** `zypper`

**Install command:** `zypper --non-interactive refresh; zypper --non-interactive install --no-recommends ${PACKAGES}`
**Cleanup command:** `zypper clean --all || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- openSUSE Tumbleweed is a rolling release with the latest packages.
- Excellent Qt6 support, typically ships the latest Qt6 version.

---

## Arch Linux

### Arch Latest

**File:** `dockerfiles/arch-latest.Dockerfile`
**FROM:** `archlinux:latest`
**Package manager:** `pacman`

```dockerfile
FROM archlinux:latest

RUN set -eux; \
  ... \
  pacman -Syu --noconfirm --needed ${PACKAGES}; \
  ... \
  pacman -Scc --noconfirm || true; \
  ...
```

**Install command:** `pacman -Syu --noconfirm --needed ${PACKAGES}`
**Cleanup command:** `pacman -Scc --noconfirm || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Notes:**
- Rolling release with bleeding-edge packages.
- `-Syu` performs a full system update before installing, which is mandatory on
  Arch to avoid partial-upgrade breakage.
- `--needed` skips packages that are already installed at the latest version.
- Qt6 packages: `qt6-base`, `qt6-tools`, etc.
- `pacman -Scc --noconfirm` removes all cached packages and unused repositories,
  significantly reducing image size.

---

## Gentoo

### Gentoo Stage 3

**File:** `dockerfiles/gentoo-stage3.Dockerfile`
**FROM:** `gentoo/stage3:latest`
**Package manager:** `emerge`

```dockerfile
FROM gentoo/stage3:latest

RUN set -eux; \
  ... \
  emerge --sync; emerge ${PACKAGES}; \
  ... \
  true; \
  export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/usr/libexec/qt6:/opt/qt6/bin:/root/.nix-profile/bin"; \
  ...
```

**Install command:** `emerge --sync; emerge ${PACKAGES}`
**Cleanup command:** `true` (no-op)
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, **`/usr/libexec/qt6`**, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Qt6 extra checks:** Also checks `/usr/libexec/qt6/qmake` and `/usr/libexec/qt6/qtpaths`
**Notes:**
- Gentoo Stage 3 is a minimal Gentoo installation from which packages are compiled.
- `emerge --sync` refreshes the Portage tree (package database).
- Package compilation from source makes builds **very slow** compared to binary distributions.
- Has the extended `/usr/libexec/qt6` path.
- Qt6 packages in Gentoo: `dev-qt/qtbase`, `dev-qt/qttools`, etc.

---

## NixOS / Nix

### Nix Latest

**File:** `dockerfiles/nix-latest.Dockerfile`
**FROM:** `nixos/nix:latest`
**Package manager:** `nix-env`

```dockerfile
FROM nixos/nix:latest

RUN set -eux; \
  ... \
  nix-env -iA ${PACKAGES}; \
  ... \
  nix-collect-garbage -d || true; \
  export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/usr/libexec/qt6:/opt/qt6/bin:/root/.nix-profile/bin"; \
  ...
```

**Install command:** `nix-env -iA ${PACKAGES}`
**Cleanup command:** `nix-collect-garbage -d || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, **`/usr/libexec/qt6`**, `/opt/qt6/bin`, **`/root/.nix-profile/bin`**
**Qt6 extra checks:** Also checks `/usr/libexec/qt6/qmake` and `/usr/libexec/qt6/qtpaths`
**Notes:**
- Uses the Nix package manager, not a traditional FHS layout.
- `-iA` installs packages by attribute path (e.g., `nixpkgs.qt6.qtbase`).
- `nix-collect-garbage -d` removes old generations and unreferenced store paths.
- `/root/.nix-profile/bin` is the primary binary path for Nix-installed packages.
  This is where `qmake6` / `qtpaths6` would be found after `nix-env -iA`.
- The `-l` flag in `SHELL ["/bin/sh", "-lc"]` is especially important here to
  source `/root/.nix-profile/etc/profile.d/nix.sh`.

---

## Void Linux

### Void Latest

**File:** `dockerfiles/void-latest.Dockerfile`
**FROM:** `voidlinux/voidlinux:latest`
**Package manager:** `xbps`

```dockerfile
FROM voidlinux/voidlinux:latest

RUN set -eux; \
  ... \
  xbps-install -Sy ${PACKAGES}; \
  ... \
  xbps-remove -O || true; \
  export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/usr/libexec/qt6:/opt/qt6/bin:/root/.nix-profile/bin"; \
  ...
```

**Install command:** `xbps-install -Sy ${PACKAGES}`
**Cleanup command:** `xbps-remove -O || true`
**Qt6 PATH:** `/usr/lib/qt6/bin`, `/usr/lib64/qt6/bin`, **`/usr/libexec/qt6`**, `/opt/qt6/bin`, `/root/.nix-profile/bin`
**Qt6 extra checks:** Also checks `/usr/libexec/qt6/qmake` and `/usr/libexec/qt6/qtpaths`
**Notes:**
- Void Linux is an independent rolling-release distribution.
- XBPS (X Binary Package System) is Void's native package manager.
- `-S` syncs the repository data; `-y` assumes yes to all prompts.
- `xbps-remove -O` removes orphaned packages and old cached downloads.
- Qt6 packages in Void: `qt6-base-devel`, `qt6-tools-devel`, etc.

---

## Package Manager Summary Table

| Package Manager | Command in Dockerfile                                                         | Cleanup Command                     | Distros using it          |
|-----------------|-------------------------------------------------------------------------------|-------------------------------------|---------------------------|
| `apt`           | `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`      | `rm -rf /var/lib/apt/lists/*`       | Debian, Ubuntu, Devuan, Kali |
| `dnf`           | `dnf install -y ${PACKAGES}`                                                  | `dnf clean all \|\| true`           | Fedora, Alma, CentOS, Rocky, Oracle, AL2023 |
| `yum`           | `yum install -y ${PACKAGES}`                                                  | `yum clean all \|\| true`           | Amazon Linux 2            |
| `apk`           | `apk add --no-cache ${PACKAGES}`                                              | `true`                              | Alpine                    |
| `zypper`        | `zypper --non-interactive refresh; zypper --non-interactive install --no-recommends ${PACKAGES}` | `zypper clean --all \|\| true` | openSUSE                  |
| `pacman`        | `pacman -Syu --noconfirm --needed ${PACKAGES}`                                | `pacman -Scc --noconfirm \|\| true` | Arch                      |
| `emerge`        | `emerge --sync; emerge ${PACKAGES}`                                            | `true`                              | Gentoo                    |
| `nix-env`       | `nix-env -iA ${PACKAGES}`                                                     | `nix-collect-garbage -d \|\| true`  | NixOS/Nix                 |
| `xbps`          | `xbps-install -Sy ${PACKAGES}`                                                | `xbps-remove -O \|\| true`         | Void                      |

---

## Related Documentation

- [Overview](overview.md) — project summary and image inventory
- [Architecture](architecture.md) — structural details
- [Qt6 Verification](qt6-verification.md) — the verification gate
- [CI/CD Integration](ci-cd-integration.md) — workflow details
- [Creating New Images](creating-new-images.md) — adding new distributions
- [Troubleshooting](troubleshooting.md) — debugging builds
