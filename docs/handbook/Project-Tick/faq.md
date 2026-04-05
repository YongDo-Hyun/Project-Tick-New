# Project Tick — FAQ & Troubleshooting

## General Questions

### What is Project Tick?

Project Tick is a unified monorepo containing MeshMC (a custom Minecraft
launcher), supporting libraries, infrastructure tooling, and developer
utilities. The project encompasses 15+ sub-projects spanning C, C++23, Rust,
Java, Python, JavaScript, Nix, Shell, and Dockerfile.

### Why a monorepo?

A single repository provides:

- **Atomic changes** — Cross-cutting modifications (e.g., updating neozip and
  MeshMC together) land in a single commit.
- **Unified CI** — One orchestrator workflow dispatches per-component CI based
  on changed files.
- **Shared tooling** — Formatting, linting, license compliance, and Git hooks
  apply uniformly.
- **Simplified dependency management** — Internal libraries (json4cpp,
  tomlplusplus, libnbtplusplus, neozip) are consumed as source, not as
  external packages.

### Why C++23?

MeshMC uses C++23 for:

- `std::expected` for error handling without exceptions in performance paths
- `std::format` / `std::print` for type-safe formatting
- `std::ranges` improvements for cleaner data transformations
- Deducing `this` for CRTP replacement
- `if consteval` for compile-time branching

Minimum compiler support: Clang 18+, GCC 14+, MSVC 17.10+.

### Why fork zlib-ng instead of using it directly?

neozip is a maintained fork of zlib-ng with Project Tick-specific
modifications:

- Build system integration with MeshMC's CMake configuration
- Custom SIMD dispatch tuned for the project's use patterns
- Consistent licensing and REUSE annotations
- Patches carried forward as the upstream project evolves

### Why fork Vim?

MNV extends Vim with modern development features while maintaining backward
compatibility. The fork is maintained in-tree to allow tight integration with
the Project Tick development workflow.

### Why fork nlohmann/json?

json4cpp is a fork of nlohmann/json maintained for:

- Build system compatibility with MeshMC
- Consistent REUSE/SPDX license annotations
- Controlled update cadence synchronized with launcher releases

### What platforms does MeshMC support?

| Platform        | Architecture | Status        |
|-----------------|-------------|---------------|
| Linux           | x86_64      | Full support  |
| Linux           | aarch64     | Full support  |
| macOS           | x86_64      | Full support  |
| macOS           | aarch64     | Full support  |
| Windows         | x86_64      | Full support  |
| Windows         | aarch64     | Full support  |
| WSL             | —           | Not supported |

### How do I contact the project?

- **Security issues**: yongdohyun@projecttick.org (see `SECURITY.md`)
- **General inquiries**: Open a GitHub issue or discussion
- **Trademark questions**: yongdohyun@projecttick.org

---

## Build Problems

### CMake: "Could not find a configuration file for package Qt6"

Qt 6 is not installed or not in the CMake search path.

**Solution (Linux — Package Manager):**

```bash
# Debian/Ubuntu
sudo apt install qt6-base-dev qt6-5compat-dev

# Fedora
sudo dnf install qt6-qtbase-devel qt6-qt5compat-devel

# Arch
sudo pacman -S qt6-base qt6-5compat
```

**Solution (Nix):**

```bash
# From the meshmc/ directory:
nix develop
# Or if direnv is set up:
cd meshmc/  # .envrc activates automatically
```

**Solution (macOS):**

```bash
brew install qt@6
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
```

### CMake: "CMake 3.28 or higher is required"

MeshMC requires CMake 3.28+ for C++23 module support.

**Solution:**

```bash
# Nix (provides latest CMake)
nix develop

# pip (if system package is too old)
pip install --user cmake

# Snap
sudo snap install cmake --classic
```

### CMake: "Could NOT find ECM"

Extra CMake Modules (ECM) from the KDE project is required.

```bash
# Debian/Ubuntu
sudo apt install extra-cmake-modules

# Fedora
sudo dnf install extra-cmake-modules

# Arch
sudo pacman -S extra-cmake-modules

# Nix
# Already included in flake.nix devShell
```

### "In-source builds are not allowed"

MeshMC's CMake configuration prohibits building in the source directory.

**Solution:**

```bash
cd meshmc/
cmake -B build -S .
cmake --build build
```

Never run `cmake .` directly in the source tree. Always use `-B <builddir>`.

### "compiler does not support C++23"

Your compiler is too old. MeshMC requires:
- Clang 18+
- GCC 14+
- MSVC 17.10+ (Visual Studio 2022 17.10)

**Solution:**

```bash
# Check your compiler version
clang++ --version
g++ --version

# Use Nix for guaranteed Clang 22
nix develop
```

### neozip: configure fails on macOS

macOS may not have all required build tools.

```bash
# Install Xcode command line tools
xcode-select --install

# Use Homebrew for missing dependencies
brew install autoconf automake libtool
```

### forgewrapper: Gradle build fails

Ensure you use the Gradle wrapper, not a system-installed Gradle:

```bash
cd forgewrapper/
./gradlew build    # Unix
gradlew.bat build  # Windows
```

The Gradle wrapper (`gradlew`) downloads the correct Gradle version
automatically. Do not use `gradle build` with a system installation.

### genqrcode: autogen.sh fails

Install Autotools prerequisites:

```bash
# Debian/Ubuntu
sudo apt install autoconf automake libtool pkg-config

# Then bootstrap:
cd genqrcode/
./autogen.sh
./configure
make
```

### corebinutils: GNUmakefile errors

corebinutils requires BSD make extensions and may not build with all GNU Make
versions. Run the configure script first:

```bash
cd corebinutils/
./configure
make -f GNUmakefile
```

### cgit: missing Git submodule

cgit requires a bundled Git source tree as a submodule.

```bash
git submodule update --init --recursive cgit/git/
cd cgit/
make
```

If the `cgit/git/` directory is empty, the submodule was not initialized.

### MeshMC: vcpkg dependencies fail (Windows)

```powershell
# Ensure vcpkg is bootstrapped
cd meshmc
.\bootstrap.cmd

# Or manually:
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install --triplet x64-windows
```

---

## CI Problems

### CI: "REUSE lint failed"

Every file in the repository must have a license annotation. Check which
files are non-compliant:

```bash
reuse lint
```

Fix by adding the file to `REUSE.toml`:

```toml
[[annotations]]
path = ["path/to/new/file"]
SPDX-FileCopyrightText = "YYYY Your Name"
SPDX-License-Identifier = "MIT"
```

Or add an SPDX header to the file itself:

```c
// SPDX-FileCopyrightText: 2025 Your Name
// SPDX-License-Identifier: GPL-3.0-or-later
```

### CI: "DCO check failed"

Your commit is missing the `Signed-off-by` line.

**Fix the last commit:**

```bash
git commit --amend -s
```

**Fix older commits (interactive rebase):**

```bash
git rebase -i HEAD~N
# Mark commits as "edit", then for each:
git commit --amend -s
git rebase --continue
```

**Prevent future failures:**

```bash
# Always use -s flag:
git commit -s -m "feat(meshmc): add feature"
```

### CI: "Conventional Commits lint failed"

Commit messages must follow the Conventional Commits format:

```
type(scope): description

[body]

[footer]
Signed-off-by: Name <email>
```

Valid types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`,
`build`, `ci`, `chore`, `revert`.

**Bad:**
```
Fixed the bug
Update README
```

**Good:**
```
fix(meshmc): resolve crash on instance deletion
docs: update getting-started guide
```

### CI: "checkpatch failed"

The lefthook pre-commit hook runs checkpatch on C/C++ changes. Common
issues:

- Trailing whitespace
- Mixed tabs and spaces
- Missing newline at end of file
- Lines exceeding column limit

### CI: "treefmt check failed"

treefmt checks that all files are formatted correctly. Run locally:

```bash
# Via Nix
nix run .#treefmt

# Or format individual files
clang-format -i file.cpp                    # C/C++
black file.py                               # Python
rustfmt file.rs                             # Rust
nixfmt file.nix                             # Nix
shfmt -w file.sh                            # Shell
```

### CI: workflow not triggered

Check whether your changes match the workflow's path filters. The monolithic
`ci.yml` uses change detection to only run relevant sub-project CI:

```yaml
# File changes are analyzed in:
# .github/actions/change-analysis/
```

If you modify only documentation, MeshMC build workflows will not trigger.
This is intentional.

---

## Git & Repository Questions

### How do I clone the repository?

```bash
git clone --recurse-submodules https://github.com/AetherMC/Project-Tick.git
cd Project-Tick
```

The `--recurse-submodules` flag is critical — cgit depends on a bundled Git
source tree.

### How do I update submodules?

```bash
git submodule update --init --recursive
```

### How do I set up the development environment?

**Option A — Nix (recommended):**

```bash
nix develop
```

**Option B — bootstrap script:**

```bash
# Linux
./bootstrap.sh

# Windows
bootstrap.cmd
```

**Option C — Manual installation:**

See the [Getting Started](getting-started.md) guide for per-platform
instructions.

### How do I run the bootstrap script?

```bash
chmod +x bootstrap.sh
./bootstrap.sh
```

The script detects your distribution (Debian/Ubuntu, Fedora/RHEL, SUSE,
Arch, macOS) and verifies that required dependencies are installed. It does
**not** install packages automatically — it reports what is missing.

### How do I contribute?

1. Fork the repository
2. Create a feature branch: `git checkout -b feature-my-change`
3. Make changes following coding standards
4. Commit with sign-off: `git commit -s`
5. Push and open a pull request
6. Sign the CLA (PT-CLA-2.0) if first contribution

See [Contributing](contributing.md) for full details.

### What is the AI contribution policy?

AI-assisted contributions are accepted under these rules:

- AI-generated code must be reviewed and understood by the contributor
- Commit messages must include `Assisted-by: <tool>` in the trailer
- The human contributor is legally responsible for the code
- AI-generated test data and documentation are explicitly welcome
- Fully autonomous AI commits without human review are not accepted

### How do I use lefthook?

lefthook is configured in `lefthook.yml` and runs Git hooks automatically:

```bash
# Install lefthook
go install github.com/evilmartians/lefthook@latest

# Or via Nix
nix profile install nixpkgs#lefthook

# Install hooks
lefthook install
```

After installation, REUSE lint and checkpatch run automatically on
`git commit`.

---

## Library Questions

### How do I use json4cpp in my CMake project?

```cmake
# As a subdirectory (recommended in monorepo)
add_subdirectory(json4cpp)
target_link_libraries(my_target PRIVATE nlohmann_json::nlohmann_json)
```

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

json j = json::parse(R"({"key": "value"})");
std::string val = j["key"];
```

### How do I use tomlplusplus?

```cpp
#include <toml++/toml.hpp>

auto config = toml::parse_file("config.toml");
auto value = config["section"]["key"].value<std::string>();
```

### How do I use libnbtplusplus?

```cpp
#include <nbt/nbt.hpp>

// Read NBT from file
std::ifstream file("level.dat", std::ios::binary);
auto tag = nbt::io::read_compound(file);

// Access data
auto& level = tag->at("Data").as<nbt::tag_compound>();
std::string name = level.at("LevelName").as<nbt::tag_string>().get();
```

### How do I use neozip?

neozip is API-compatible with zlib:

```c
#include <zlib.h>

// Compress
z_stream strm = {0};
deflateInit(&strm, Z_DEFAULT_COMPRESSION);
// ... standard zlib API usage
deflateEnd(&strm);
```

### How does forgewrapper work?

ForgeWrapper uses Java SPI to provide a file detector for Forge's installer:

```java
// META-INF/services/io.github.zekerzhayard.forgewrapper.installer.detector.IFileDetector
// Points to the SPI implementation class
```

MeshMC uses forgewrapper as a runtime dependency when launching Forge-based
Minecraft instances.

---

## Nix Questions

### Nix: "error: experimental feature 'flakes' is disabled"

Enable flakes in your Nix configuration:

```bash
# ~/.config/nix/nix.conf
experimental-features = nix-command flakes
```

Or pass the flag:

```bash
nix --experimental-features 'nix-command flakes' develop
```

### Nix: "error: getting status of /nix/store/..."

The Nix store may be corrupted. Try:

```bash
nix-store --verify --check-contents --repair
```

### Nix: flake.lock is outdated

```bash
nix flake update
git add flake.lock
git commit -s -m "build(nix): update flake.lock"
```

### Nix: how do I update CI's pinned nixpkgs?

```bash
cd ci/
./update-pinned.sh
git add pinned.json
git commit -s -m "ci: update pinned nixpkgs"
```

---

## tickborg Questions

### What is tickborg?

tickborg is Project Tick's CI bot, forked from ofborg. It listens for GitHub
events via AMQP (RabbitMQ) and performs automated builds and tests.

### How do I use tickborg commands?

In a pull request comment:

```
@tickbot build meshmc     # Build MeshMC
@tickbot test meshmc      # Build and test MeshMC
@tickbot eval meshmc      # Evaluate MeshMC Nix expression
```

### How do I deploy tickborg locally?

```bash
cd ofborg/
cp example.config.json config.json
# Edit config.json with your AMQP credentials
docker-compose up -d
```

See `ofborg/DEPLOY.md` for full deployment instructions.

---

## Platform-Specific Questions

### Can I build on WSL?

No. Project Tick explicitly does not support WSL for development. Use:

- **Native Linux** for Linux builds
- **Native Windows with MSVC** for Windows builds
- **macOS** for macOS builds

The `bootstrap.sh` script will exit with an error if it detects a WSL
environment.

### macOS: "xcrun: error: invalid active developer path"

Install Xcode command line tools:

```bash
xcode-select --install
```

### Windows: "bootstrap.cmd fails with Scoop not found"

Install Scoop first:

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression
```

Then re-run `bootstrap.cmd`.

---

## Licensing Questions

### Which license applies to my contribution?

Contributions inherit the license of the component you are modifying:
- MeshMC → GPL-3.0-or-later
- neozip → Zlib
- json4cpp → MIT
- tomlplusplus → MIT
- libnbtplusplus → LGPL-3.0-or-later
- cgit → GPL-2.0-only
- forgewrapper → MIT
- meta → MS-PL
- corebinutils → BSD (varies by utility)

### How do I check license compliance?

```bash
reuse lint
```

This command checks that every file has proper SPDX annotations, either via
file headers or `REUSE.toml` entries.

### What is PT-CLA-2.0?

The Project Tick Contributor License Agreement, version 2.0. It grants
the project a perpetual, irrevocable license to use your contribution. You
retain copyright ownership. The CLA must be signed once before your first
PR can be merged.
