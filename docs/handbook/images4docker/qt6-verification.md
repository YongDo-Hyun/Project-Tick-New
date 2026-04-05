# images4docker — Qt6 Verification

## Purpose

Every Dockerfile in images4docker includes a mandatory **Qt6 toolchain
verification gate**.  This gate runs at the end of the `RUN` instruction,
after all packages have been installed.  If the gate fails, the entire
Docker build fails — there is **no fallback to Qt5** and no option to skip
the check.

This ensures that every image published to `ghcr.io/project-tick-infra/images/`
is guaranteed to have a working Qt6 toolchain available.

---

## How the Gate Works

### Step 1: PATH Extension

Before checking for Qt6 binaries, the `PATH` environment variable is extended
to include all known Qt6 installation directories:

**Standard path extension** (most distributions):

```sh
export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin"
```

**Extended path** (Amazon Linux 2023, Gentoo, NixOS, Oracle Linux, Void Linux):

```sh
export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/usr/libexec/qt6:/opt/qt6/bin:/root/.nix-profile/bin"
```

The extended variant adds `/usr/libexec/qt6` for distributions where Qt6
installs its binaries under `libexec`.

### Step 2: Binary Search

The gate checks for the presence of Qt6 binaries using a multi-pronged approach:

**Command-based checks** (via `command -v` — checks `$PATH`):

```sh
command -v qmake6 >/dev/null 2>&1
command -v qmake-qt6 >/dev/null 2>&1
command -v qtpaths6 >/dev/null 2>&1
```

**Absolute path checks** (via `[ -x ... ]` — checks specific filesystem locations):

Standard set:
```sh
[ -x /usr/lib/qt6/bin/qmake ]
[ -x /usr/lib64/qt6/bin/qmake ]
[ -x /usr/lib/qt6/bin/qtpaths ]
[ -x /usr/lib64/qt6/bin/qtpaths ]
```

Extended set (Oracle Linux, Gentoo, NixOS, Amazon Linux 2023, Void Linux):
```sh
[ -x /usr/libexec/qt6/qmake ]
[ -x /usr/libexec/qt6/qtpaths ]
```

### Step 3: Pass or Fail

All checks are combined with `||` (logical OR).  If **any single check**
succeeds, the gate passes:

```sh
if command -v qmake6 >/dev/null 2>&1 \
   || command -v qmake-qt6 >/dev/null 2>&1 \
   || command -v qtpaths6 >/dev/null 2>&1 \
   || [ -x /usr/lib/qt6/bin/qmake ] \
   || [ -x /usr/lib64/qt6/bin/qmake ] \
   || [ -x /usr/lib/qt6/bin/qtpaths ] \
   || [ -x /usr/lib64/qt6/bin/qtpaths ]; then
    true;
  else
    echo "Qt6 toolchain not found" >&2;
    exit 1;
  fi
```

If **all checks fail**, the gate prints "Qt6 toolchain not found" to stderr
and exits with code 1, which causes the Docker `RUN` instruction to fail
and aborts the build.

---

## Actual Dockerfile Snippet

Here is the exact verification code as it appears in the Dockerfiles (shown
in formatted form — the actual files have it on a single `RUN` line):

```sh
set -eux

# ... package installation ...

# PATH extension
export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin"

# Qt6 verification gate
if command -v qmake6 >/dev/null 2>&1 \
     || command -v qmake-qt6 >/dev/null 2>&1 \
     || command -v qtpaths6 >/dev/null 2>&1 \
     || [ -x /usr/lib/qt6/bin/qmake ] \
     || [ -x /usr/lib64/qt6/bin/qmake ] \
     || [ -x /usr/lib/qt6/bin/qtpaths ] \
     || [ -x /usr/lib64/qt6/bin/qtpaths ]; then
  true
else
  echo "Qt6 toolchain not found" >&2
  exit 1
fi
```

---

## Why These Specific Binaries?

### qmake6

`qmake6` is the Qt 6 version of the qmake build system.  On most distributions,
the `qt6-base-dev` or `qt6-qtbase-devel` package installs it as `qmake6`.

### qmake-qt6

Some distributions (especially Debian-based ones) install the Qt6 qmake as
`qmake-qt6` instead of `qmake6`.  The `-qt6` suffix is a Debian packaging
convention to allow multiple Qt versions to coexist.

### qtpaths6

`qtpaths6` is a Qt6 utility that reports installation paths (plugin directory,
library directory, etc.).  It is a lightweight Qt6 binary that confirms the
Qt6 runtime is properly installed, without needing a full build tool like qmake.

### Why check absolute paths too?

On some distributions, Qt6 binaries are installed to non-standard locations
that may not be in `$PATH`:

| Path                          | Used by                                          |
|-------------------------------|--------------------------------------------------|
| `/usr/lib/qt6/bin/qmake`     | Debian, Ubuntu (32-bit or arch-independent)      |
| `/usr/lib64/qt6/bin/qmake`   | Fedora, RHEL family (64-bit lib directory)       |
| `/usr/libexec/qt6/qmake`     | Oracle Linux, Gentoo, Amazon Linux 2023, Void    |
| `/usr/lib/qt6/bin/qtpaths`   | Debian, Ubuntu                                   |
| `/usr/lib64/qt6/bin/qtpaths` | Fedora, RHEL family                              |
| `/usr/libexec/qt6/qtpaths`   | Oracle Linux, Gentoo, Amazon Linux 2023, Void    |
| `/opt/qt6/bin/`              | Custom Qt6 installations (from source or installer)|
| `/root/.nix-profile/bin/`    | NixOS (Nix profile symlinks)                     |

---

## Which Dockerfiles Use Which Path Variant?

### Standard Qt6 paths (7 check locations, 4 PATH dirs)

Used by 30 Dockerfiles:

- All **AlmaLinux** images (alma-9, alma-10)
- All **Alpine** images (alpine-319 through alpine-latest)
- **Arch** (arch-latest)
- All **CentOS Stream** images (centos-stream9, centos-stream10)
- All **Debian** images (bookworm, bookworm-slim, bullseye, bullseye-slim, stable-slim, trixie-slim)
- All **Devuan** images (devuan-chimaera, devuan-daedalus)
- All **Fedora** images (fedora-40, fedora-41, fedora-42, fedora-latest)
- **Kali** (kali-rolling)
- All **openSUSE** images (opensuse-leap-155, opensuse-leap-156, opensuse-tumbleweed)
- All **Rocky** images (rocky-9, rocky-10)
- All **Ubuntu** images (ubuntu-2004, ubuntu-2204, ubuntu-2404, ubuntu-latest)
- **Amazon Linux 2** (amazonlinux-2)

```sh
export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin"

# Checks: qmake6, qmake-qt6, qtpaths6 (via command -v)
#   plus: /usr/lib/qt6/bin/qmake, /usr/lib64/qt6/bin/qmake,
#          /usr/lib/qt6/bin/qtpaths, /usr/lib64/qt6/bin/qtpaths
```

### Extended Qt6 paths (9 check locations, 5 PATH dirs)

Used by 6 Dockerfiles:

- **Amazon Linux 2023** (amazonlinux-2023)
- **Gentoo** (gentoo-stage3)
- **NixOS** (nix-latest)
- **Oracle Linux** 8, 9, 10 (oraclelinux-8, oraclelinux-9, oraclelinux-10)
- **Void Linux** (void-latest)

```sh
export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/usr/libexec/qt6:/opt/qt6/bin:/root/.nix-profile/bin"

# Checks: qmake6, qmake-qt6, qtpaths6 (via command -v)
#   plus: /usr/lib/qt6/bin/qmake, /usr/lib64/qt6/bin/qmake,
#          /usr/lib/qt6/bin/qtpaths, /usr/lib64/qt6/bin/qtpaths,
#          /usr/libexec/qt6/qmake, /usr/libexec/qt6/qtpaths
```

---

## Images Excluded from Active CI Matrix

The README notes that approximately **35 of 40** images are in the active CI
build matrix.  The ~5 excluded images are those where Qt6 packages are not
reliably available:

| Image                            | Reason for exclusion                                |
|----------------------------------|-----------------------------------------------------|
| `amazonlinux-2`                  | Based on RHEL 7 era; no Qt6 in default repos       |
| `debian-bullseye` / `bullseye-slim` | Debian 11 shipped Qt 5.15, not Qt6              |
| `devuan-chimaera`                | Based on Debian Bullseye, same Qt6 limitation       |
| `ubuntu-2004`                    | Ubuntu 20.04 does not ship Qt6                      |

These images are still maintained in the repository for potential future use
(e.g., if Qt6 becomes available via backports or PPAs), but they are not built
in the regular CI workflow.

---

## Failure Behaviour

When the Qt6 gate fails:

1. The `echo "Qt6 toolchain not found" >&2` message is printed to stderr.
2. `exit 1` terminates the shell with a non-zero exit code.
3. The `RUN` instruction fails.
4. Docker aborts the build.
5. The CI workflow reports a build failure for that image target.
6. The failed image is **not** pushed to the container registry.

Because `set -eux` is active at the top of the `RUN` block:
- `-e`: Exit immediately if any command fails.
- `-u`: Treat unset variables as errors.
- `-x`: Print each command before executing (useful for debugging in CI logs).

---

## Why No Qt5 Fallback?

The project has made a deliberate decision to require Qt6:

1. **Qt5 is end-of-life** — Qt 5.15 LTS support ended.  New features and
   security fixes only go into Qt6.
2. **API consistency** — supporting both Qt5 and Qt6 would require conditional
   compilation paths, increasing maintenance burden.
3. **Clear signal** — if a distribution cannot provide Qt6, it is too old to
   be a supported build target.

---

## Verifying Qt6 Locally

To test whether Qt6 would be found in a specific image, you can run:

```bash
docker run --rm <image> sh -c '
  export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin"
  echo "qmake6:    $(command -v qmake6 2>/dev/null || echo NOT FOUND)"
  echo "qmake-qt6: $(command -v qmake-qt6 2>/dev/null || echo NOT FOUND)"
  echo "qtpaths6:  $(command -v qtpaths6 2>/dev/null || echo NOT FOUND)"
  for p in /usr/lib/qt6/bin/qmake /usr/lib64/qt6/bin/qmake \
           /usr/lib/qt6/bin/qtpaths /usr/lib64/qt6/bin/qtpaths \
           /usr/libexec/qt6/qmake /usr/libexec/qt6/qtpaths; do
    [ -x "$p" ] && echo "Found: $p" || echo "Missing: $p"
  done
'
```

---

## Related Documentation

- [Overview](overview.md) — project summary
- [Architecture](architecture.md) — Dockerfile template structure
- [Base Images](base-images.md) — per-image deep dive
- [CI/CD Integration](ci-cd-integration.md) — how the workflow builds and verifies
- [Troubleshooting](troubleshooting.md) — debugging Qt6 failures
