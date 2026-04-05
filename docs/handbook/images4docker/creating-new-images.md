# images4docker — Creating New Images

## Overview

This guide explains how to add a new distribution or version to images4docker.
The process is straightforward because every Dockerfile follows an identical
template — you only need to know the base image reference and the distribution's
package manager.

---

## Prerequisites

Before adding a new image, verify:

1. **Qt6 availability** — the distribution must have Qt6 packages in its
   repositories (default, EPEL, backports, or a reliable third-party source).
   If Qt6 is not available, the image will fail the verification gate and
   cannot be used.

2. **Official Docker image** — the distribution must publish an official (or
   well-maintained) Docker image on Docker Hub, Quay.io, or another public
   registry.

3. **Active support** — the distribution version should be actively maintained.
   End-of-life versions should not be added.

---

## Step-by-Step: Adding a New Dockerfile

### Step 1: Determine the Base Image

Find the official Docker image reference on Docker Hub or the distribution's
documentation.  Examples:

| Distribution        | Docker Hub reference              |
|---------------------|-----------------------------------|
| Debian Forky        | `debian:forky` or `debian:14`     |
| Ubuntu 26.04        | `ubuntu:26.04`                    |
| Fedora 43           | `fedora:43`                       |
| Alpine 3.23         | `alpine:3.23`                     |
| Rocky Linux 11      | `rockylinux/rockylinux:11`        |
| openSUSE Leap 16.0  | `opensuse/leap:16.0`              |

### Step 2: Identify the Package Manager

| Package Manager | Command in template                                                        | Clean command                      |
|-----------------|----------------------------------------------------------------------------|------------------------------------|
| `apt`           | `apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}`   | `rm -rf /var/lib/apt/lists/*`      |
| `dnf`           | `dnf install -y ${PACKAGES}`                                               | `dnf clean all \|\| true`          |
| `yum`           | `yum install -y ${PACKAGES}`                                               | `yum clean all \|\| true`          |
| `apk`           | `apk add --no-cache ${PACKAGES}`                                           | `true`                             |
| `zypper`        | `zypper --non-interactive refresh; zypper --non-interactive install --no-recommends ${PACKAGES}` | `zypper clean --all \|\| true` |
| `pacman`        | `pacman -Syu --noconfirm --needed ${PACKAGES}`                             | `pacman -Scc --noconfirm \|\| true`|
| `emerge`        | `emerge --sync; emerge ${PACKAGES}`                                         | `true`                             |
| `nix-env`       | `nix-env -iA ${PACKAGES}`                                                  | `nix-collect-garbage -d \|\| true` |
| `xbps`          | `xbps-install -Sy ${PACKAGES}`                                             | `xbps-remove -O \|\| true`        |

### Step 3: Determine the Qt6 Path Variant

Most distributions use the **standard path**:

```sh
export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin"
```

If the distro installs Qt6 binaries under `/usr/libexec/qt6/`, use the
**extended path** and add extra checks.  Currently, this applies to:
Oracle Linux, Amazon Linux 2023, Gentoo, NixOS, Void Linux.

### Step 4: Choose the Filename

Follow the naming convention:

```
<distro>-<version>.Dockerfile
```

Rules:
- Lower-case distro name.
- Version dots stripped for numbers: `3.23` → `323`, `26.04` → `2604`.
- Use `latest` for rolling-release tags.
- Use hyphenated variants for slim/special tags: `bookworm-slim`.
- Capital `D` in `.Dockerfile`.

Examples:
- `alpine-323.Dockerfile`
- `ubuntu-2604.Dockerfile`
- `fedora-43.Dockerfile`
- `debian-forky-slim.Dockerfile`

### Step 5: Create the Dockerfile

Copy an existing Dockerfile from the same package-manager family and change
only the `FROM` line.

For example, to add Fedora 43, copy `fedora-42.Dockerfile`:

```dockerfile
# syntax=docker/dockerfile:1.7
FROM fedora:43

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     dnf install -y ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     dnf clean all || true;   fi;   export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin";   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || [ -x /usr/lib/qt6/bin/qmake ] || [ -x /usr/lib64/qt6/bin/qmake ] || [ -x /usr/lib/qt6/bin/qtpaths ] || [ -x /usr/lib64/qt6/bin/qtpaths ]; then     true;   else     echo "Qt6 toolchain not found" >&2;     exit 1;   fi

CMD ["/bin/sh"]
```

**The only change is `FROM fedora:43`** — everything else is identical to the
other Fedora Dockerfiles.

### Step 6: Test Locally

Build the image locally to verify it works:

```bash
docker build \
  --file dockerfiles/fedora-43.Dockerfile \
  --build-arg PACKAGES="qt6-qtbase-devel qt6-qttools-devel cmake gcc gcc-c++ make" \
  --tag test-fedora-43 \
  .
```

If the build completes without "Qt6 toolchain not found", the image passes
the verification gate.

### Step 7: Add to the CI Matrix

Update `.github/workflows/build.yml` to include the new image in the build
matrix.  Add an entry with:

- The Dockerfile path
- The target image name and tag
- The `PACKAGES` build argument value

### Step 8: Commit and Push

```bash
git add dockerfiles/fedora-43.Dockerfile
git commit -s -m "images4docker: add Fedora 43"
git push
```

The workflow will trigger automatically and build the new image.

---

## Adding an Entirely New Distribution

If the distribution uses a package manager not yet represented, you need to:

1. **Determine the install command** — how to install packages non-interactively
   with automatic dependency resolution.

2. **Determine the cleanup command** — how to remove package caches to minimise
   image size.

3. **Create the Dockerfile** using the universal template structure.

4. **Check Qt6 binary paths** — run an interactive container from the base image,
   install Qt6 packages, and find where `qmake6` / `qtpaths6` are located:

   ```bash
   docker run --rm -it <base_image>:<tag> sh
   # Inside the container:
   <install Qt6 packages>
   find / -name 'qmake*' -o -name 'qtpaths*' 2>/dev/null
   ```

5. **Choose the path variant** — if Qt6 binaries are in `/usr/libexec/qt6/`,
   use the extended path.  Otherwise, use the standard path.

---

## Adding a Slim Variant

For distributions that offer slim/minimal Docker images (currently only Debian),
you can add a slim variant:

1. Copy the full variant's Dockerfile.
2. Change the `FROM` tag to the slim variant (e.g., `debian:trixie-slim`).
3. Name the file accordingly (e.g., `debian-trixie-slim.Dockerfile`).

The package installation and Qt6 verification are identical — slim variants
just start with fewer pre-installed packages.

---

## Retiring an Image

When a distribution version reaches end-of-life:

1. Remove the Dockerfile from `dockerfiles/`.
2. Remove the matrix entry from `.github/workflows/build.yml`.
3. The image remains in GHCR (it is not automatically deleted).
4. Downstream CI jobs that reference the image will continue to work until
   GHCR retention policies remove it.

If you want to keep the Dockerfile for historical reference but stop building
it, remove it from the workflow matrix but leave the file in the repository.

---

## Checklist for New Images

- [ ] Base image exists on Docker Hub / Quay.io / other public registry
- [ ] Qt6 packages are available in the distribution's repositories
- [ ] Dockerfile created with correct `FROM` reference
- [ ] Dockerfile uses correct package manager command
- [ ] Correct Qt6 path variant (standard or extended)
- [ ] File named according to convention: `<distro>-<version>.Dockerfile`
- [ ] Local build tested successfully
- [ ] Qt6 verification gate passes
- [ ] Workflow matrix updated with new entry
- [ ] `PACKAGES` build argument determined and documented
- [ ] Committed with `git commit -s` (signed-off)
- [ ] Push triggers successful CI build

---

## Common Mistakes

### Wrong package manager

Each distribution family has its own package manager.  Using `apt-get` in a
Fedora Dockerfile will fail immediately.  Always copy from a Dockerfile in
the same family.

### Missing repository enablement

Some RHEL-family distributions require enabling CRB/PowerTools or EPEL before
Qt6 packages are available.  Use `CUSTOM_INSTALL` for this:

```sh
CUSTOM_INSTALL="dnf config-manager --enable crb && dnf install -y epel-release && dnf install -y qt6-qtbase-devel ..."
```

### Forgetting the Dockerfile syntax directive

Every file must start with:

```dockerfile
# syntax=docker/dockerfile:1.7
```

This must be the very first line — no blank lines or comments before it.

### Using a non-existent base tag

Before creating the Dockerfile, verify the tag exists:

```bash
docker pull <base_image>:<tag>
```

If the pull fails, the tag does not exist and the CI build will fail.

### CRLF line endings

The `.gitattributes` enforces LF line endings, but if you create files on
Windows without proper Git configuration, CRLF characters can sneak in and
cause shell script failures inside the container.

---

## Template Reference

### For apt-based distributions

```dockerfile
# syntax=docker/dockerfile:1.7
FROM <base_image>:<tag>

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     apt-get update; apt-get install -y --no-install-recommends ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     rm -rf /var/lib/apt/lists/*;   fi;   export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin";   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || [ -x /usr/lib/qt6/bin/qmake ] || [ -x /usr/lib64/qt6/bin/qmake ] || [ -x /usr/lib/qt6/bin/qtpaths ] || [ -x /usr/lib64/qt6/bin/qtpaths ]; then     true;   else     echo "Qt6 toolchain not found" >&2;     exit 1;   fi

CMD ["/bin/sh"]
```

### For dnf-based distributions

```dockerfile
# syntax=docker/dockerfile:1.7
FROM <base_image>:<tag>

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     dnf install -y ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     dnf clean all || true;   fi;   export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin";   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || [ -x /usr/lib/qt6/bin/qmake ] || [ -x /usr/lib64/qt6/bin/qmake ] || [ -x /usr/lib/qt6/bin/qtpaths ] || [ -x /usr/lib64/qt6/bin/qtpaths ]; then     true;   else     echo "Qt6 toolchain not found" >&2;     exit 1;   fi

CMD ["/bin/sh"]
```

### For apk-based distributions

```dockerfile
# syntax=docker/dockerfile:1.7
FROM <base_image>:<tag>

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     apk add --no-cache ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     true;   fi;   export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin";   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || [ -x /usr/lib/qt6/bin/qmake ] || [ -x /usr/lib64/qt6/bin/qmake ] || [ -x /usr/lib/qt6/bin/qtpaths ] || [ -x /usr/lib64/qt6/bin/qtpaths ]; then     true;   else     echo "Qt6 toolchain not found" >&2;     exit 1;   fi

CMD ["/bin/sh"]
```

---

## Related Documentation

- [Overview](overview.md) — project summary
- [Architecture](architecture.md) — Dockerfile template structure
- [Base Images](base-images.md) — existing images
- [Qt6 Verification](qt6-verification.md) — the verification gate
- [CI/CD Integration](ci-cd-integration.md) — workflow details
- [Troubleshooting](troubleshooting.md) — debugging builds
