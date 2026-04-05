# images4docker — Troubleshooting

## Overview

This document covers common issues encountered when building, pushing, or
using images4docker images, and how to diagnose and resolve them.

---

## Build Failures

### "Qt6 toolchain not found"

**Symptom:** The Docker build fails with:

```
Qt6 toolchain not found
```

**Cause:** The Qt6 verification gate ran after package installation and could
not find any of the expected binaries (`qmake6`, `qmake-qt6`, `qtpaths6`) in
any of the searched paths.

**Diagnosis:**

1. Check that the correct `PACKAGES` build argument was passed.  For example,
   on Debian/Ubuntu, the package is `qt6-base-dev`, not `qt6-base`:

   ```bash
   docker build \
     --file dockerfiles/debian-bookworm.Dockerfile \
     --build-arg PACKAGES="qt6-base-dev" \
     --tag test .
   ```

2. Check that Qt6 is actually available in the distribution's repositories.
   Run an interactive container:

   ```bash
   docker run --rm -it debian:bookworm sh
   apt-get update
   apt-cache search qt6
   ```

3. For RHEL-family distributions, check if CRB/PowerTools or EPEL needs to be
   enabled.  Use `CUSTOM_INSTALL`:

   ```bash
   docker build \
     --file dockerfiles/alma-9.Dockerfile \
     --build-arg CUSTOM_INSTALL="dnf config-manager --enable crb && dnf install -y epel-release qt6-qtbase-devel" \
     --tag test .
   ```

4. Verify the Qt6 binary location inside the container:

   ```bash
   docker run --rm -it <base_image>:<tag> sh
   # Install Qt6 packages manually, then:
   find / -name 'qmake*' -o -name 'qtpaths*' 2>/dev/null
   ```

   If the binaries are in an unexpected path, the Dockerfile may need the
   extended path variant (with `/usr/libexec/qt6`).

**Resolution:**
- Ensure correct Qt6 package names for the distribution.
- Enable required repositories (CRB, EPEL, backports).
- If the distribution does not provide Qt6 at all, it should be excluded from
  the active CI matrix.

---

### "manifest not found" or "pull access denied"

**Symptom:** The build fails immediately at the `FROM` instruction:

```
ERROR: pull access denied, repository does not exist or may require authentication
```

or:

```
ERROR: manifest for <image>:<tag> not found
```

**Cause:** The upstream base image tag no longer exists.  This can happen when:
- A distribution reaches end-of-life and its Docker image is removed.
- A distribution renames or reorganises its official images.
- A registry changes its namespace (as CentOS did from Docker Hub to Quay.io).

**Diagnosis:**

```bash
docker pull <base_image>:<tag>
```

If this fails, the tag is no longer available.

**Resolution:**
- Check the distribution's official Docker Hub / Quay.io page for the current
  tag names.
- Update the `FROM` line in the Dockerfile.
- If the distribution is EOL, remove the Dockerfile and its workflow matrix entry.

---

### Package installation fails

**Symptom:** The `dnf install`, `apt-get install`, or other package manager
command fails with "No package found" or similar.

**Cause:** Package names vary between distribution versions.  For example:
- Fedora uses `qt6-qtbase-devel`, Debian uses `qt6-base-dev`.
- Some RHEL versions need `qt6-qtbase-devel`, others `qt6-qtbase-devel`.
- Alpine uses `qt6-qtbase-dev` (with musl, not glibc).

**Diagnosis:**

Run an interactive container and search for packages:

```bash
# Debian/Ubuntu
docker run --rm -it debian:bookworm sh -c 'apt-get update && apt-cache search qt6'

# Fedora/RHEL
docker run --rm -it fedora:42 sh -c 'dnf search qt6'

# Alpine
docker run --rm -it alpine:3.22 sh -c 'apk search qt6'

# openSUSE
docker run --rm -it opensuse/tumbleweed:latest sh -c 'zypper search qt6'

# Arch
docker run --rm -it archlinux:latest sh -c 'pacman -Ss qt6'
```

**Resolution:**
- Use the correct package names for the distribution.
- If a package was renamed, update the `PACKAGES` in the workflow matrix.
- If a package is missing, try `CUSTOM_INSTALL` to enable additional repositories.

---

### "set -u: unbound variable"

**Symptom:** The build fails with a message about an unbound variable.

**Cause:** The `set -u` flag (part of `set -eux`) causes the shell to fail
on any reference to an unset variable.  However, all `ARG` values default to
empty strings (`ARG PACKAGES=`), so this should not normally occur.

**Diagnosis:** Check that all `ARG` declarations are present in the Dockerfile
and that they have default values:

```dockerfile
ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=
```

**Resolution:** Ensure all four `ARG` lines are present with `=` (defaulting
to empty string).

---

## Push Failures

### GHCR authentication failure

**Symptom:** `docker push` fails with "authentication required" or
"denied: permission denied".

**Cause:**
- The `GITHUB_TOKEN` does not have `packages: write` permission.
- The workflow permissions are not configured correctly.

**Resolution:**
- Ensure the workflow has:
  ```yaml
  permissions:
    packages: write
    contents: read
  ```
- Verify the repository settings allow GitHub Actions to push packages.

---

### Image size unexpectedly large

**Symptom:** A pushed image is much larger than expected.

**Cause:** The cleanup command did not run, or it was overridden with an
empty / incorrect value.

**Diagnosis:**

Check the image size:

```bash
docker images --format "{{.Repository}}:{{.Tag}} {{.Size}}" | grep project-tick
```

Compare with the base image size:

```bash
docker images --format "{{.Repository}}:{{.Tag}} {{.Size}}" | grep <base_image>
```

**Resolution:**
- Verify the cleanup command is correct for the package manager.
- Check that `CLEAN_CMD` is not set to an empty string (which would skip
  the default cleanup).
- For apt-based images, ensure `rm -rf /var/lib/apt/lists/*` runs after install.
- For dnf-based images, ensure `dnf clean all` runs.

---

## Runtime Issues

### "command not found" in CI jobs

**Symptom:** A CI job using a images4docker image reports "command not found"
for tools like `cmake`, `gcc`, or `make`.

**Cause:** The image was built without the complete `PACKAGES` list, or the
build argument was incorrect.

**Diagnosis:**

```bash
docker run --rm ghcr.io/project-tick-infra/images/<name>:<tag> which cmake
```

**Resolution:**
- Check the `PACKAGES` build argument in the workflow matrix.
- Rebuild the image with the correct package list.

---

### Qt6 binaries not in PATH

**Symptom:** CI jobs report `qmake6: command not found` even though the
image was built successfully.

**Cause:** The `export PATH=...` in the Dockerfile's `RUN` instruction only
applies during the build.  It does **not** persist as an environment variable
in the final image.

**Diagnosis:**

```bash
docker run --rm ghcr.io/project-tick-infra/images/<name>:<tag> sh -c 'echo $PATH'
```

If the Qt6 paths are not in the output, they need to be set at runtime.

**Resolution:**
- In CI jobs, explicitly set the PATH:
  ```yaml
  env:
    PATH: "$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin"
  ```
- Or use the full path to the binary: `/usr/lib64/qt6/bin/qmake6`.
- Consider adding an `ENV PATH=...` line to the Dockerfiles (this is a
  design decision — currently not done to keep images generic).

---

### Alpine: "Error relocating" or linking errors

**Symptom:** Compiled binaries on Alpine fail at runtime with relocation
errors or "not found" for shared libraries.

**Cause:** Alpine uses `musl libc`, not `glibc`.  Binaries compiled on
glibc-based distributions are not compatible.

**Resolution:**
- Ensure all dependencies are compiled against musl on Alpine.
- This is not an images4docker issue per se — it is an Alpine-specific constraint.
- Use a glibc-based image (Debian, Fedora, etc.) if glibc compatibility is required.

---

## Debugging Techniques

### Interactive Build

Add `--progress=plain` to see the full build output:

```bash
docker build --progress=plain \
  --file dockerfiles/<distro>.Dockerfile \
  --build-arg PACKAGES="..." \
  --tag test .
```

### Shell into a Failed Build

If the build fails, start a container from the base image and manually
run the install commands:

```bash
docker run --rm -it <base_image>:<tag> sh
```

Then manually execute the RUN commands to see where they fail.

### Check Build Arguments

Print the build arguments to verify they are being passed correctly:

```bash
docker build \
  --file dockerfiles/<distro>.Dockerfile \
  --build-arg PACKAGES="qt6-base-dev cmake" \
  --build-arg UPDATE_CMD="echo UPDATE_CMD is set" \
  --tag test .
```

The `set -x` (from `set -eux`) will print each command, including the
expanded variable values.

### Compare with a Working Image

If a new Dockerfile fails but a similar one works, diff them:

```bash
diff dockerfiles/fedora-42.Dockerfile dockerfiles/fedora-43.Dockerfile
```

The only difference should be the `FROM` line.

### Inspect a Built Image

```bash
# Check image layers
docker history ghcr.io/project-tick-infra/images/<name>:<tag>

# Check image metadata
docker inspect ghcr.io/project-tick-infra/images/<name>:<tag>

# Get a shell in the image
docker run --rm -it ghcr.io/project-tick-infra/images/<name>:<tag> sh
```

---

## Known Limitations

### No Qt5 fallback

If a distribution cannot provide Qt6, there is no option to fall back to Qt5.
The image will fail the verification gate.  This is intentional.

### No multi-arch support

Currently all images are built for `linux/amd64` only.  There is no
`linux/arm64` or other architecture support.

### PATH not persisted

The `export PATH=...` in the `RUN` instruction does not create a persistent
`ENV` in the image.  CI jobs may need to set `PATH` themselves.

### Single-layer constraint

All package installation happens in a single `RUN` instruction.  If you need
to debug which specific package fails, you must do so interactively rather
than by inserting intermediate `RUN` instructions (which would change the
template structure).

---

## Getting Help

1. Check the CI workflow logs (GitHub Actions) for the exact error message.
2. Reproduce locally with `docker build --progress=plain`.
3. Run an interactive container from the base image to test package availability.
4. Consult the distribution's documentation for Qt6 package names.
5. Review the [Base Images](base-images.md) documentation for distribution-specific notes.

---

## Related Documentation

- [Overview](overview.md) — project summary
- [Architecture](architecture.md) — Dockerfile template structure
- [Base Images](base-images.md) — per-distribution details
- [Qt6 Verification](qt6-verification.md) — the verification gate
- [CI/CD Integration](ci-cd-integration.md) — workflow details
- [Creating New Images](creating-new-images.md) — adding new distributions
