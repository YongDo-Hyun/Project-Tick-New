# MNV — Contributing

## How to Contribute

MNV welcomes patches in any form.  The project's `CONTRIBUTING.md` states:

> Patches are welcome in whatever form. Discussions about patches happen on
> the mnv-dev mailing list.

### Preferred Channels

1. **GitHub Pull Requests** — PRs on the `Project-Tick/Project-Tick` repository.
   These trigger CI automatically and are forwarded to the mailing list.
2. **mnv-dev mailing list** — Send a unified diff as an attachment.  Initial
   posts are moderated.

---

## Setting Up a Development Environment

### Clone the repository

```bash
git clone https://github.com/Project-Tick/Project-Tick.git
cd Project-Tick/mnv
```

### Build with debug flags

Use the `all-interp` or `sanitize` CMake preset for development:

```bash
cmake --preset sanitize
cmake --build build/sanitize -j$(nproc)
```

Or manually:

```bash
mkdir build && cd build
cmake .. -DMNV_DEBUG=ON -DMNV_LEAK_CHECK=ON -DMNV_BUILD_TESTS=ON
cmake --build . -j$(nproc)
```

### Run the test suite

```bash
cd build
ctest
```

Or run specific test categories:

```bash
ctest -R json_test          # Unit test
ctest -L scripts            # Script tests (src/testdir/)
ctest -L indent             # Indent tests
ctest -L syntax             # Syntax tests
```

### Legacy Autoconf build for testing

```bash
cd src
make
make test
```

---

## Contribution Guidelines

### Always add tests

From `CONTRIBUTING.md`:

> Please always add a test, if possible. All new functionality should be tested
> and bug fixes should be tested for regressions: the test should fail before
> the fix and pass after the fix.

Tests live in `src/testdir/`.  Look at recent patches for examples.  Use
`:help testing` inside MNV for the testing framework documentation.

### Code style

Follow the existing code style (see the `code-style.md` handbook page):

- Hard tabs in C files, `ts=8 sts=4 sw=4 noet`.
- Function return type on a separate line.
- Allman braces.
- Feature guards with `#ifdef FEAT_*`.

The CI runs `test_codestyle.mnv` which checks for style violations.
**Contributions must pass this check.**

### Commit messages

- Write clear, descriptive commit messages.
- Reference issue numbers where applicable.
- One logical change per commit.

### Signed-off-by

While not strictly required, it is recommended to sign off commits using the
Developer Certificate of Origin (DCO):

```bash
git commit -s
```

This adds a `Signed-off-by:` trailer confirming you have the right to submit
the change under the project's license.

The maintainer (`@chrisbra`) usually adds missing `Signed-off-by` trailers
when merging.

### AI-generated code

From `CONTRIBUTING.md`:

> When using AI for contributions, please disclose this. Any AI-generated code
> must follow the MNV code style. In particular, test_codestyle.mnv must not
> report any failures.

Additional rules:
- Ensure changes are properly tested.
- Do not submit a single PR addressing multiple unrelated issues.

---

## License

Contributions are distributed under the **MNV license** (see `COPYING.md`
and `LICENSE`).  By submitting a change you agree to this.

> Providing a change to be included implies that you agree with this and your
> contribution does not cause us trouble with trademarks or patents. There is
> no CLA to sign.

---

## Reporting Issues

### GitHub Issues

Use [GitHub Issues](https://github.com/Project-Tick/Project-Tick/issues/new/choose)
for actual bugs.

### Before reporting

1. Reproduce with a clean configuration:

   ```bash
   mnv --clean
   ```

2. Describe exact reproduction steps.  Don't say "insert some text" — instead:
   `ahere is some text<Esc>`.

3. Check the todo file: `:help todo`.

### Appropriate places for discussion

- Not sure if it's a bug? Use the **mnv-dev mailing list** or
  [reddit.com/r/mnv](https://reddit.com/r/mnv) or
  [StackExchange](https://vi.stackexchange.com/).
- Feature requests and design discussions belong on the mailing list or GitHub
  issues.

---

## Runtime Files (Syntax, Indent, Ftplugins)

If you find a problem with a syntax, indent, or ftplugin file:

1. Check the file header for the **maintainer's** name, email, or GitHub handle.
2. Also check the `MAINTAINERS` file.
3. Contact the maintainer directly.
4. The maintainer sends updates to the MNV project for distribution.

If the maintainer does not respond, use the mailing list or GitHub issues.

### MNV9 in runtime files

Whether to use MNV9 script is up to the maintainer.  For files maintained in
the main repository, preserve compatibility with Neovim if possible.  Wrap
MNV9-specific code in a guard.

---

## CI and Automated Checks

Every pull request triggers:

| CI System | Platform | What it checks |
|---|---|---|
| GitHub Actions | Linux, macOS | Build, tests, coverage |
| Appveyor | Windows | MSVC build, tests |
| Cirrus CI | FreeBSD | Build, tests |
| Codecov | — | Coverage reporting (noisy, can be ignored) |
| Coverity Scan | — | Static analysis |

### CI configuration

CI helper scripts live in the `ci/` directory:

- `ci/config.mk.sed` — configure options for CI builds.
- `ci/config.mk.clang.sed` / `ci/config.mk.gcc.sed` — compiler-specific
  variants.
- `ci/setup-xvfb.sh` — sets up Xvfb for GUI tests.
- `ci/remove_snap.sh` — removes snap packages that interfere with CI.
- `ci/pinned-pkgs` — pinned package versions for reproducibility.

---

## Development Workflow

### 1. Fork and branch

```bash
git checkout -b fix-my-issue
```

### 2. Make changes and test

```bash
cd build
cmake --build . -j$(nproc)
ctest -R relevant_test
```

### 3. Check code style

Build and run the code-style test:

```bash
cd src/testdir
make test_codestyle.mnv
```

### 4. Commit with sign-off

```bash
git commit -s -m "Fix: description of the fix

Detailed explanation if needed.
Closes #NNNN"
```

### 5. Push and create PR

```bash
git push origin fix-my-issue
```

Open a Pull Request on GitHub.

---

## Architecture for New Contributors

New contributors should familiarize themselves with:

| File | Purpose |
|---|---|
| `src/main.c` | Entry point — read the startup sequence |
| `src/mnv.h` | Master header — see the include hierarchy |
| `src/feature.h` | Feature tiers and optional feature defines |
| `src/structs.h` | All major data structures |
| `src/globals.h` | Global variables |
| `src/ex_docmd.c` | Ex command dispatcher |
| `src/normal.c` | Normal-mode command dispatcher |

See the `architecture.md` handbook page for a complete subsystem map.

The `README.md` in `src/` provides additional guidance on the source tree
layout.

---

## Communication

| Channel | Purpose |
|---|---|
| GitHub Issues | Bug reports, feature requests |
| mnv-dev mailing list | Design discussions, patch review |
| `#mnv` on Libera.Chat | Real-time chat |
| reddit.com/r/mnv | Community discussion |
| StackExchange (vi.stackexchange.com) | Q&A |

---

*This document describes MNV 10.0 as of build 287 (2026-04-03).*
