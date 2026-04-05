# images4docker — Architecture

## Repository Structure

images4docker follows a deliberately flat, single-purpose architecture.  There
are no build scripts, no Makefiles, no helper utilities.  Every file in the
repository serves exactly one role.

```
images4docker/
├── .gitattributes          # Line-ending enforcement (LF everywhere)
├── .gitignore              # Ignore *.log, *.tmp, .env
├── LICENSE                 # GPL-3.0-or-later (full text)
├── LICENSES/
│   └── GPL-3.0-or-later.txt   # REUSE-compliant license copy
├── README.md               # Project overview and notes
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

Total: **40 Dockerfiles**, **1 README**, **1 LICENSE pair**, **2 git config files**.

---

## Dockerfile Naming Convention

Every Dockerfile follows a strict naming pattern:

```
<distro>-<version_or_tag>.Dockerfile
```

### Rules

1. **Distro name** is the short, lower-case distribution identifier:
   `alma`, `alpine`, `amazonlinux`, `arch`, `centos-stream`, `debian`,
   `devuan`, `fedora`, `gentoo`, `kali`, `nix`, `opensuse-leap`,
   `opensuse-tumbleweed`, `oraclelinux`, `rocky`, `ubuntu`, `void`.

2. **Version** is the numeric version with dots stripped, or a keyword:
   - Numeric: `9`, `10`, `319` (for 3.19), `2004` (for 20.04), `155` (for 15.5)
   - Keywords: `latest`, `rolling`, `stage3`, `stream9`, `stream10`
   - Variants: `bookworm-slim`, `bullseye-slim`, `stable-slim`, `trixie-slim`

3. **Extension** is always `.Dockerfile` (capital D), not `.dockerfile`.

### Examples

| File name                        | Distribution    | Version / Tag        |
|----------------------------------|-----------------|----------------------|
| `alma-9.Dockerfile`             | AlmaLinux       | 9                    |
| `alpine-322.Dockerfile`         | Alpine Linux    | 3.22                 |
| `debian-bookworm-slim.Dockerfile` | Debian        | Bookworm (12), slim  |
| `centos-stream10.Dockerfile`    | CentOS Stream   | 10                   |
| `opensuse-leap-156.Dockerfile`  | openSUSE Leap   | 15.6                 |
| `ubuntu-2404.Dockerfile`        | Ubuntu          | 24.04                |
| `void-latest.Dockerfile`        | Void Linux      | latest               |

---

## The Universal Dockerfile Template

Every single Dockerfile in the repository shares the same structural template.
The only differences between files are:

1. The `FROM` base image reference.
2. The default package-manager command used when `CUSTOM_INSTALL` is not set.
3. The default cleanup command.
4. Minor variations in the Qt6 binary search path.

### Template Anatomy

```dockerfile
# syntax=docker/dockerfile:1.7
FROM <base_image>:<tag>

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux; \
  if [ -n "${UPDATE_CMD}" ]; then \
    sh -lc "${UPDATE_CMD}"; \
  fi; \
  if [ -n "${CUSTOM_INSTALL}" ]; then \
    sh -lc "${CUSTOM_INSTALL}"; \
  elif [ -n "${PACKAGES}" ]; then \
    <package_manager> install <flags> ${PACKAGES}; \
  fi; \
  if [ -n "${CLEAN_CMD}" ]; then \
    sh -lc "${CLEAN_CMD}"; \
  else \
    <default_cleanup>; \
  fi; \
  export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin"; \
  <qt6_verification_gate>

CMD ["/bin/sh"]
```

### Template Sections Explained

#### 1. Syntax Directive

```dockerfile
# syntax=docker/dockerfile:1.7
```

Every file begins with this BuildKit syntax directive.  This enables:
- Heredoc support (`<<EOF`)
- Improved caching behaviour
- `RUN --mount` options (though not currently used)
- Better error messages during builds

#### 2. FROM Statement

```dockerfile
FROM <base_image>:<tag>
```

Each Dockerfile has a single, non-parameterised `FROM`.  The image reference is
hardcoded — there is no `ARG`-based base image selection.  This is intentional:
every Dockerfile builds exactly one image from exactly one base.

The `FROM` references use public registries:

| Registry                | Used by                                    |
|-------------------------|--------------------------------------------|
| Docker Hub (implicit)   | AlmaLinux, Alpine, Amazon Linux, Arch,     |
|                         | Debian, Fedora, Gentoo, Kali, NixOS,       |
|                         | openSUSE, Oracle Linux, Ubuntu, Void       |
| `quay.io`               | CentOS Stream (`quay.io/centos/centos`)    |
| Docker Hub (explicit)   | Devuan (`devuan/devuan`), Rocky            |
|                         | (`rockylinux/rockylinux`),                 |
|                         | Void (`voidlinux/voidlinux`)               |

#### 3. Build Arguments

```dockerfile
ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=
```

All four arguments default to empty strings.  They are the injection points
through which the CI workflow customises each build:

| Argument          | Purpose                                                     | Example value                           |
|-------------------|-------------------------------------------------------------|-----------------------------------------|
| `PACKAGES`        | Space-separated list of packages to install                 | `qt6-base-dev cmake gcc g++`            |
| `CUSTOM_INSTALL`  | Arbitrary shell command that replaces the default install    | `dnf config-manager --enable crb && dnf install -y qt6-qtbase-devel` |
| `UPDATE_CMD`      | Shell command run before package installation                | `apt-get update`                        |
| `CLEAN_CMD`       | Shell command run after installation to reduce image size    | `rm -rf /var/lib/apt/lists/*`           |

Priority logic:
1. If `CUSTOM_INSTALL` is non-empty, it is executed instead of the package manager.
2. Otherwise, if `PACKAGES` is non-empty, the native package manager installs them.
3. If neither is set, nothing is installed (but the Qt6 check still runs and will fail).

#### 4. Shell Override

```dockerfile
SHELL ["/bin/sh", "-lc"]
```

The default Docker shell is `["/bin/sh", "-c"]`.  The `-l` flag forces a login
shell, which ensures:
- `/etc/profile` and `/etc/profile.d/*.sh` are sourced.
- `PATH` extensions from the distribution's login scripts are available.
- NixOS profile paths (`/root/.nix-profile/bin`) are activated.

#### 5. The RUN Block

The entire build logic is a single `RUN` instruction.  This is deliberate — it
creates a single Docker layer, minimising image size and avoiding intermediate
layers that would persist deleted files.

The `RUN` block executes in this order:

```
┌──────────────────────┐
│  set -eux            │    Fail on errors, undefined vars, print commands
├──────────────────────┤
│  UPDATE_CMD?         │    Optional: pre-install update (apt-get update, etc.)
├──────────────────────┤
│  CUSTOM_INSTALL?     │    If set: run arbitrary install command
│    or PACKAGES?      │    Else if set: run native pkg manager with PACKAGES
├──────────────────────┤
│  CLEAN_CMD?          │    If set: run custom cleanup
│    or default clean  │    Else: run distro-appropriate cleanup
├──────────────────────┤
│  export PATH=...     │    Extend PATH with Qt6 binary locations
├──────────────────────┤
│  Qt6 verification    │    Check for qmake6/qmake-qt6/qtpaths6 binaries
│  gate                │    FAILS BUILD if not found
└──────────────────────┘
```

#### 6. CMD

```dockerfile
CMD ["/bin/sh"]
```

Every image defaults to a shell.  CI jobs override this with their own
`entrypoint` or `command` specifications, so the `CMD` is effectively a
debug/interactive fallback.

---

## Package Manager Dispatch

The package-manager command in the `RUN` block varies per distribution family.
Here is the exact command used by each group:

### apt-based (Debian, Ubuntu, Devuan, Kali)

```sh
apt-get update; apt-get install -y --no-install-recommends ${PACKAGES}
```

- `--no-install-recommends` keeps images lean by skipping suggested packages.
- Default cleanup: `rm -rf /var/lib/apt/lists/*`

### dnf-based (Fedora, AlmaLinux, CentOS Stream, Rocky, Oracle Linux, Amazon Linux 2023)

```sh
dnf install -y ${PACKAGES}
```

- Default cleanup: `dnf clean all || true`

### yum-based (Amazon Linux 2)

```sh
yum install -y ${PACKAGES}
```

- Default cleanup: `yum clean all || true`

### apk-based (Alpine Linux)

```sh
apk add --no-cache ${PACKAGES}
```

- `--no-cache` means no index files are persisted.
- Default cleanup: `true` (no-op, since apk --no-cache handles it).

### zypper-based (openSUSE Leap, Tumbleweed)

```sh
zypper --non-interactive refresh; zypper --non-interactive install --no-recommends ${PACKAGES}
```

- `--non-interactive` prevents prompts.
- `--no-recommends` skips recommended (but not required) packages.
- Default cleanup: `zypper clean --all || true`

### pacman-based (Arch Linux)

```sh
pacman -Syu --noconfirm --needed ${PACKAGES}
```

- `-Syu` does a full system upgrade before installing.
- `--needed` skips already-installed packages.
- Default cleanup: `pacman -Scc --noconfirm || true`

### emerge-based (Gentoo)

```sh
emerge --sync; emerge ${PACKAGES}
```

- `--sync` refreshes the Portage tree before installing.
- Default cleanup: `true` (no-op).

### nix-env-based (NixOS/Nix)

```sh
nix-env -iA ${PACKAGES}
```

- `-iA` installs by attribute path from nixpkgs.
- Default cleanup: `nix-collect-garbage -d || true`

### xbps-based (Void Linux)

```sh
xbps-install -Sy ${PACKAGES}
```

- `-S` syncs the repository index.
- `-y` assumes yes to prompts.
- Default cleanup: `xbps-remove -O || true`

---

## Qt6 Binary Search Paths

After package installation, every Dockerfile extends `PATH` to include
distribution-specific Qt6 binary directories.  There are two variants:

### Standard Path Extension (most distros)

```sh
export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin"
```

Used by: AlmaLinux, Alpine, Arch, CentOS Stream, Debian, Devuan, Fedora,
Kali, openSUSE, Rocky, Ubuntu.

### Extended Path (distros with /usr/libexec/qt6)

```sh
export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/usr/libexec/qt6:/opt/qt6/bin:/root/.nix-profile/bin"
```

Used by: Amazon Linux 2023, Gentoo, NixOS, Oracle Linux, Void Linux.

The `/usr/libexec/qt6` path is added for distributions where Qt6 installs its
binaries under `libexec` rather than `lib/qt6/bin`.

---

## Base Image Selection Strategy

The choice of base images follows these principles:

### Version Pinning

- **LTS releases** are pinned to specific versions: `ubuntu:20.04`, `ubuntu:22.04`,
  `ubuntu:24.04`, `debian:bookworm`, `alpine:3.19`, etc.
- **Rolling releases** use `latest` tags: `archlinux:latest`, `fedora:latest`,
  `alpine:latest`, `opensuse/tumbleweed:latest`.
- **Dual coverage**: where possible, both a pinned version and a `latest` tag
  are maintained (e.g., `alpine-321.Dockerfile` + `alpine-latest.Dockerfile`).

### Registry Selection

- **Docker Hub** is the primary registry for most images.
- **Quay.io** is used for CentOS Stream because the official CentOS images
  are hosted there: `quay.io/centos/centos:stream9`.
- **Namespaced images** are used where distributions publish under their own
  Docker Hub organisation: `devuan/devuan`, `rockylinux/rockylinux`,
  `voidlinux/voidlinux`, `kalilinux/kali-rolling`, `gentoo/stage3`.

### Slim vs Full Variants

For Debian, both full and slim variants are maintained:
- `debian:bookworm` — full image with documentation, man pages, extra utilities.
- `debian:bookworm-slim` — minimal image, roughly half the size.

The slim variants are preferred for CI because they download faster, but full
variants are kept for cases where build scripts expect standard utilities.

---

## Configuration Files

### .gitattributes

```
# images4docker
* text=auto eol=lf
*.Dockerfile text
```

- Forces LF line endings on all text files.
- Explicitly marks `*.Dockerfile` as text to ensure proper diff handling.
- Prevents CRLF corruption when contributors use Windows.

### .gitignore

```
# images4docker
*.log
*.tmp
.env
```

- Ignores build logs and temporary files.
- Ignores `.env` files that might contain registry credentials.

---

## Design Principles

### Single-layer images

Each Dockerfile has exactly one `RUN` instruction.  This means:
- The final image has the base image's layers plus exactly one additional layer.
- No intermediate layers persist files that are later deleted (which would bloat
  the image even though the files are not visible).

### No COPY or ADD

None of the Dockerfiles copy any files from the build context.  All configuration
is done via `ARG` values injected at build time.  This means:
- The Docker build context is effectively empty.
- Builds are fast because no files need to be sent to the Docker daemon.
- The Dockerfiles are entirely self-contained.

### No ENTRYPOINT

Images use `CMD ["/bin/sh"]` without an `ENTRYPOINT`.  This allows CI jobs to
override the command freely without needing `--entrypoint`.

### No EXPOSE or VOLUME

These images are build environments, not services.  There are no network ports
to expose and no data volumes to mount.

### No USER directive

All images run as `root`.  CI builds typically need root to install packages
and access system directories.  Security isolation is handled at the container
runtime level (Docker, Podman, etc.), not inside the image.

### No HEALTHCHECK

These are ephemeral CI images, not long-running services.  Health checks would
add unnecessary complexity.

---

## Image Lifecycle

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────────┐
│  Upstream base   │────▶│  Dockerfile      │────▶│  Built image        │
│  (Docker Hub /   │     │  (in this repo)  │     │  (GHCR)             │
│   Quay.io)       │     │                  │     │                     │
└─────────────────┘     └──────────────────┘     └─────────────────────┘
        │                        │                         │
        │ Daily pull             │ Push to main /          │ Used by CI
        │ (cron 03:17 UTC)       │ daily cron              │ jobs in other
        │                        │                         │ repositories
        ▼                        ▼                         ▼
   New upstream      ──▶   Rebuild triggered   ──▶   New image pushed
   tag available            by workflow              to ghcr.io
```

1. Upstream distributions publish new base images.
2. The daily cron or a push to `main` triggers the GitHub Actions workflow.
3. The workflow builds each Dockerfile with the appropriate `--build-arg` values.
4. The Qt6 verification gate passes or fails the build.
5. Successful images are pushed to `ghcr.io/project-tick-infra/images/`.
6. Other Project Tick CI jobs pull these images as their build containers.

---

## Related Documentation

- [Overview](overview.md) — project summary
- [Base Images](base-images.md) — per-image deep dive
- [Qt6 Verification](qt6-verification.md) — the verification gate
- [CI/CD Integration](ci-cd-integration.md) — workflow details
- [Creating New Images](creating-new-images.md) — adding distributions
- [Troubleshooting](troubleshooting.md) — debugging builds
