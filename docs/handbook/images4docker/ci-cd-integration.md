# images4docker — CI/CD Integration

## Overview

images4docker is purpose-built for CI/CD.  The entire repository exists to
produce pre-baked Docker images that other Project Tick repositories consume
as build containers.  This document covers how the images are built, pushed,
and consumed.

---

## GitHub Actions Workflow

### Workflow File

The CI workflow is defined in `.github/workflows/build.yml` (referenced in the
project README as "Repack Existing Images").

### Trigger Conditions

The workflow runs on two triggers:

#### 1. Push to `main`

Triggered when any of these paths change in a push to the `main` branch:

- `dockerfiles/*.Dockerfile` — any Dockerfile modification
- `.github/workflows/build.yml` — workflow changes
- `README.md` — documentation updates

#### 2. Scheduled Cron

```yaml
schedule:
  - cron: '17 3 * * *'   # Daily at 03:17 UTC
```

The daily schedule ensures that:
- Upstream base image security patches are incorporated.
- New upstream package versions are picked up.
- Images stay current even when no Dockerfiles change.

The non-standard minute (`17`) avoids the GitHub Actions "top of the hour"
congestion that causes delayed job starts.

---

## Build Process

### Build Matrix

The workflow defines a build matrix of approximately **35 targets** — the
Qt6-capable subset of the 40 Dockerfiles.  Each target specifies:

- The Dockerfile path
- The target image name and tag
- The `PACKAGES` build argument (distro-specific package list)
- Optionally, the `CUSTOM_INSTALL`, `UPDATE_CMD`, or `CLEAN_CMD` build arguments

### Build Arguments

Each distribution requires a different set of Qt6 package names.  The workflow
injects these via Docker build arguments.

#### apt-based distributions (Debian, Ubuntu, Devuan, Kali)

Typical `PACKAGES` value:

```
qt6-base-dev qt6-tools-dev qmake6 cmake gcc g++ make pkg-config
```

Notes:
- Qt6 packages require `apt-get update` first (injected via `UPDATE_CMD` or
  handled by the Dockerfile's default `apt-get update` prefix).
- `--no-install-recommends` is hardcoded in the Dockerfile.

#### dnf-based distributions (Fedora, RHEL family)

Typical `PACKAGES` value:

```
qt6-qtbase-devel qt6-qttools-devel cmake gcc gcc-c++ make pkgconfig
```

For RHEL-family distros that need CRB/PowerTools, the workflow may use
`CUSTOM_INSTALL`:

```sh
dnf config-manager --enable crb && dnf install -y epel-release && dnf install -y qt6-qtbase-devel ...
```

#### apk-based distributions (Alpine)

Typical `PACKAGES` value:

```
qt6-qtbase-dev qt6-qttools-dev cmake gcc g++ make musl-dev pkgconf
```

Notes:
- No `libsystemd-dev` equivalent (Alpine does not use systemd).
- Uses `musl-dev` instead of `libc6-dev`.

#### zypper-based distributions (openSUSE)

Typical `PACKAGES` value:

```
qt6-base-devel qt6-tools-devel cmake gcc gcc-c++ make pkg-config
```

#### pacman-based (Arch Linux)

Typical `PACKAGES` value:

```
qt6-base qt6-tools cmake gcc make pkgconf
```

#### emerge-based (Gentoo)

Typical `PACKAGES` value:

```
dev-qt/qtbase dev-qt/qttools dev-build/cmake
```

#### nix-env-based (NixOS)

Typical `PACKAGES` value:

```
nixpkgs.qt6.qtbase nixpkgs.qt6.qttools nixpkgs.cmake nixpkgs.gcc nixpkgs.gnumake
```

#### xbps-based (Void Linux)

Typical `PACKAGES` value:

```
qt6-base-devel qt6-tools-devel cmake gcc make pkg-config
```

### Docker Build Command

Each matrix entry runs a Docker build command equivalent to:

```bash
docker build \
  --file dockerfiles/<distro>-<tag>.Dockerfile \
  --build-arg PACKAGES="<package list>" \
  --build-arg CUSTOM_INSTALL="<optional custom command>" \
  --build-arg UPDATE_CMD="<optional pre-install command>" \
  --build-arg CLEAN_CMD="<optional cleanup command>" \
  --tag ghcr.io/project-tick-infra/images/<target_name>:<target_tag> \
  .
```

### BuildKit

Every Dockerfile starts with `# syntax=docker/dockerfile:1.7`, which enables
BuildKit features.  The workflow likely sets `DOCKER_BUILDKIT=1` or uses
`docker buildx build` for:

- Improved build caching
- Better error reporting
- Parallel layer execution

---

## Container Registry

### Registry URL

```
ghcr.io/project-tick-infra/images/
```

All images are pushed to the **GitHub Container Registry** (GHCR), which is
tightly integrated with GitHub Actions.

### Image Naming

The target format for pushed images is:

```
ghcr.io/project-tick-infra/images/<target_name>:<target_tag>
```

Where `<target_name>` and `<target_tag>` are derived from the Dockerfile name.
For example:

| Dockerfile                     | Image reference                                                 |
|--------------------------------|-----------------------------------------------------------------|
| `alma-9.Dockerfile`           | `ghcr.io/project-tick-infra/images/alma:9`                     |
| `alpine-321.Dockerfile`       | `ghcr.io/project-tick-infra/images/alpine:3.21`                |
| `debian-bookworm-slim.Dockerfile` | `ghcr.io/project-tick-infra/images/debian:bookworm-slim`   |
| `ubuntu-2404.Dockerfile`      | `ghcr.io/project-tick-infra/images/ubuntu:24.04`               |
| `fedora-latest.Dockerfile`    | `ghcr.io/project-tick-infra/images/fedora:latest`              |

### Authentication

The workflow authenticates to GHCR using the built-in `GITHUB_TOKEN` provided
by GitHub Actions:

```bash
echo "${{ secrets.GITHUB_TOKEN }}" | docker login ghcr.io -u ${{ github.actor }} --password-stdin
```

### Push

After a successful build (meaning the Qt6 verification gate passed), the
image is pushed:

```bash
docker push ghcr.io/project-tick-infra/images/<target_name>:<target_tag>
```

---

## Consuming the Images

### In GitHub Actions Workflows

Other Project Tick repositories can use these images as build containers:

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: ghcr.io/project-tick-infra/images/ubuntu:24.04
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: |
          cmake -B build
          cmake --build build
```

### In Docker Compose

```yaml
services:
  build:
    image: ghcr.io/project-tick-infra/images/fedora:42
    volumes:
      - .:/workspace
    working_dir: /workspace
    command: cmake -B build && cmake --build build
```

### As a FROM Base

Other Dockerfiles can extend these images:

```dockerfile
FROM ghcr.io/project-tick-infra/images/debian:bookworm-slim
RUN apt-get update && apt-get install -y additional-package
```

### Direct Docker Run

```bash
docker run --rm -v $(pwd):/workspace -w /workspace \
  ghcr.io/project-tick-infra/images/alpine:3.22 \
  cmake -B build && cmake --build build
```

---

## Build Lifecycle

```
 ┌────────────────────────────────────────────────────────────────────────┐
 │                     Trigger (push to main or cron)                     │
 └────────────────────────┬───────────────────────────────────────────────┘
                          │
                          ▼
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      Login to GHCR                                     │
 └────────────────────────┬───────────────────────────────────────────────┘
                          │
                          ▼
 ┌────────────────────────────────────────────────────────────────────────┐
 │                     Build matrix (parallel)                            │
 │                                                                        │
 │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │
 │   │  alma:9     │  │  alpine:3.21│  │  fedora:42  │  ...  (×35)      │
 │   │             │  │             │  │             │                    │
 │   │ 1. Pull base│  │ 1. Pull base│  │ 1. Pull base│                  │
 │   │ 2. Install  │  │ 2. Install  │  │ 2. Install  │                  │
 │   │ 3. Clean    │  │ 3. Clean    │  │ 3. Clean    │                  │
 │   │ 4. Qt6 gate │  │ 4. Qt6 gate │  │ 4. Qt6 gate │                  │
 │   │ 5. Tag      │  │ 5. Tag      │  │ 5. Tag      │                  │
 │   │ 6. Push     │  │ 6. Push     │  │ 6. Push     │                  │
 │   └─────────────┘  └─────────────┘  └─────────────┘                  │
 └────────────────────────────────────────────────────────────────────────┘
```

---

## Caching Strategy

### No explicit Docker cache

The current design rebuilds images from scratch on each run.  This is
intentional:

1. **Security** — pulling fresh base images ensures the latest security patches
   are included.
2. **Reproducibility** — no stale cached layers that might mask package changes.
3. **Simplicity** — no cache management, no cache invalidation bugs.

The trade-off is longer build times (~35 parallel builds), which is acceptable
for a daily cron job.

### Layer efficiency

Each Dockerfile produces a single additional layer on top of the base image.
This means:

- No intermediate layers to cache or invalidate.
- The push to GHCR transfers only the diff layer.
- If the base image hasn't changed and the packages are the same, the layer
  content will be identical (content-addressable storage deduplicates it).

---

## Monitoring and Failure Handling

### Build Failures

When a Qt6 verification fails:

1. The Docker build exits with code 1.
2. The GitHub Actions matrix job for that image is marked as failed.
3. Other matrix jobs continue (matrix builds are independent).
4. The workflow summary shows which images succeeded and which failed.

### Upstream Image Disappearance

If an upstream base image tag is removed or renamed:

1. The `FROM` instruction fails with "manifest not found".
2. The build fails for that specific target.
3. The old image remains in GHCR (it is not deleted).
4. Fix: update the Dockerfile to use the new tag, or remove the Dockerfile.

### Monitoring

Build status can be monitored via:

- GitHub Actions workflow run history
- GHCR package page (shows when images were last updated)
- Downstream CI failures (if an image is stale or missing)

---

## Security Considerations

### Image Provenance

- All Dockerfiles use official, well-known upstream base images.
- CentOS Stream images are pulled from `quay.io/centos/centos` (the official
  CentOS mirror), not unofficial sources.
- No third-party or personal Docker Hub repositories are used as bases.

### No Secrets in Images

- The Dockerfiles do not `COPY` any files from the build context.
- No `ARG` or `ENV` values contain secrets.
- The `GITHUB_TOKEN` is only used for GHCR authentication, not passed into builds.

### No Network Access at Runtime

The images are build environments.  They do not expose ports, run daemons,
or listen for connections.  Network access is only used during `docker build`
to download packages from distribution repositories.

### Daily Rebuilds

The daily cron ensures that security patches from upstream distributions are
incorporated within 24 hours.

---

## Related Documentation

- [Overview](overview.md) — project summary and image list
- [Architecture](architecture.md) — Dockerfile structure
- [Base Images](base-images.md) — per-distribution details
- [Qt6 Verification](qt6-verification.md) — the verification gate
- [Creating New Images](creating-new-images.md) — adding new distributions
- [Troubleshooting](troubleshooting.md) — debugging build failures
