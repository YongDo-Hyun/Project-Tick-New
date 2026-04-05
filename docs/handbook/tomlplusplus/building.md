# toml++ — Building

## Overview

toml++ supports multiple build modes and build systems. It can be consumed as a header-only library, a single-header drop-in, or a compiled static/shared library. The primary build system is Meson, with CMake as a first-class alternative and Visual Studio project files also provided.

---

## Build Modes

### 1. Header-Only Mode (Default)

The simplest way to use toml++. No compilation of the library itself is needed.

**Setup:**
1. Add `tomlplusplus/include` to your include paths
2. `#include <toml++/toml.hpp>` in your source files
3. Compile your project with C++17 or later

This is the default mode. The macro `TOML_HEADER_ONLY` defaults to `1`.

**Advantages:**
- Zero build configuration
- No separate library to link
- Works with any build system

**Disadvantages:**
- Every translation unit that includes toml++ compiles the full implementation
- Can increase compile times in large projects

**Example CMake integration:**
```cmake
# Add tomlplusplus as a subdirectory or fetch it
add_subdirectory(external/tomlplusplus)
target_link_libraries(my_target PRIVATE tomlplusplus::tomlplusplus)
```

### 2. Single-Header Mode

toml++ ships with a pre-amalgamated single-header file at the repository root: `toml.hpp`.

**Setup:**
1. Copy `toml.hpp` into your project
2. `#include "toml.hpp"` in your source files
3. Done

This file contains the entire library — all headers, all `.inl` implementation files — concatenated into one file. The API is identical to the multi-header version.

### 3. Compiled Library Mode

For projects where compile time matters, toml++ can be built as a compiled library.

**Setup:**

In exactly **one** translation unit, compile `src/toml.cpp`:

```cpp
// src/toml.cpp — this is the entire compiled-library source
#ifndef TOML_IMPLEMENTATION
#define TOML_IMPLEMENTATION
#endif
#ifndef TOML_HEADER_ONLY
#define TOML_HEADER_ONLY 0
#endif

#include <toml++/toml.hpp>
```

In all other translation units, define `TOML_HEADER_ONLY=0` before including the header:

```cpp
#define TOML_HEADER_ONLY 0
#include <toml++/toml.hpp>
```

Or set it project-wide via compiler flags:
```bash
g++ -DTOML_HEADER_ONLY=0 -I/path/to/tomlplusplus/include ...
```

**Advantages:**
- The parser and formatter implementations are compiled once
- Faster incremental builds
- Smaller binary size (fewer inlined copies)

**Disadvantages:**
- Requires linking the compiled translation unit
- Need to ensure `TOML_HEADER_ONLY=0` is consistent across all TUs

### 4. C++20 Modules Mode

When using CMake 3.28+ and a C++20 compiler:

```cmake
cmake -DTOMLPLUSPLUS_BUILD_MODULES=ON ..
```

Then in your source:
```cpp
import tomlplusplus;
```

Module support is experimental and requires:
- CMake ≥ 3.28
- A compiler with C++20 module support (recent GCC, Clang, or MSVC)

The module source files are in `src/modules/`.

---

## Meson Build System

Meson is the primary build system for toml++. The project file is `meson.build` at the repository root.

### Project Definition

```meson
project(
    'tomlplusplus',
    'cpp',
    license: 'MIT',
    version: '3.4.0',
    meson_version: '>=0.61.0',
    default_options: [
        'buildtype=release',
        'default_library=shared',
        'b_lto=false',
        'b_ndebug=if-release',
        'cpp_std=c++17'
    ]
)
```

### Meson Options

Options are defined in `meson_options.txt`:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `devel` | bool | `false` | Development build (implies `build_tests`, `build_examples`, `pedantic`) |
| `build_lib` | bool | `false` | Compile as a library (implied by `devel`) |
| `build_examples` | bool | `false` | Build example programs (implied by `devel`) |
| `build_tests` | bool | `false` | Build test suite (implied by `devel`) |
| `build_tt` | bool | `false` | Build toml-test encoder/decoder (implied by `devel`, disabled by `unreleased_features`) |
| `pedantic` | bool | `false` | Enable maximum compiler warnings (implied by `devel`) |
| `permissive` | bool | `false` | MSVC `/permissive` mode (default is `/permissive-`) |
| `time_trace` | bool | `false` | Enable `-ftime-trace` (Clang only) |
| `unreleased_features` | bool | `false` | Enable `TOML_UNRELEASED_FEATURES=1` |
| `generate_cmake_config` | bool | `true` | Generate a CMake package config file |
| `use_vendored_libs` | bool | `true` | Use vendored Catch2 for tests |

### Building with Meson

```bash
# Configure
meson setup build
# Or with options:
meson setup build -Dbuild_tests=true -Dbuild_examples=true

# Compile
meson compile -C build

# Run tests
meson test -C build

# Development build (builds everything, enables warnings)
meson setup build -Ddevel=true
```

### Using as a Meson Subproject

Create `subprojects/tomlplusplus.wrap`:
```ini
[wrap-git]
url = https://github.com/marzer/tomlplusplus.git
revision = v3.4.0

[provide]
tomlplusplus = tomlplusplus_dep
```

Then in your `meson.build`:
```meson
tomlplusplus_dep = dependency('tomlplusplus', version: '>=3.4.0')
executable('my_app', 'main.cpp', dependencies: [tomlplusplus_dep])
```

### Meson Library Target

When `build_lib` is true (or implied), the library is compiled:

```meson
# In the meson.build, the library creates a tomlplusplus_dep dependency
# that other targets consume
```

The compiled library defines:
- `TOML_HEADER_ONLY=0`
- `TOML_IMPLEMENTATION`

### Compiler Flag Management

The Meson build applies comprehensive compiler flags based on the detected compiler:

**Common flags:**
```
-ferror-limit=5          # Clang: max errors
-fmax-errors=5           # GCC: max errors
-fchar8_t                # Enable char8_t
```

**MSVC-specific:**
```
/bigobj                  # Large object file support
/utf-8                   # UTF-8 source encoding
/Zc:__cplusplus          # Correct __cplusplus value
/Zc:inline               # Remove unreferenced COMDAT
/Zc:externConstexpr      # External constexpr linkage
/Zc:preprocessor         # Standards-conforming preprocessor
```

**Pedantic mode** enables extensive warning flags for both GCC and Clang (`-Weverything`, `-Wcast-align`, `-Wshadow`, etc.) with targeted suppressions for unavoidable warnings (`-Wno-c++98-compat`, `-Wno-padded`, etc.).

---

## CMake Build System

### CMake Project

The `CMakeLists.txt` defines an interface (header-only) library:

```cmake
cmake_minimum_required(VERSION 3.14)

project(
    tomlplusplus
    VERSION 3.4.0
    DESCRIPTION "Header-only TOML config file parser and serializer for C++17"
    HOMEPAGE_URL "https://marzer.github.io/tomlplusplus/"
    LANGUAGES CXX
)

add_library(tomlplusplus_tomlplusplus INTERFACE)
add_library(tomlplusplus::tomlplusplus ALIAS tomlplusplus_tomlplusplus)

target_include_directories(
    tomlplusplus_tomlplusplus
    INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(tomlplusplus_tomlplusplus INTERFACE cxx_std_17)
```

### Using with CMake — Subdirectory

```cmake
add_subdirectory(path/to/tomlplusplus)
target_link_libraries(my_target PRIVATE tomlplusplus::tomlplusplus)
```

### Using with CMake — FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

target_link_libraries(my_target PRIVATE tomlplusplus::tomlplusplus)
```

### Using with CMake — find_package

If toml++ is installed system-wide or the CMake config was generated:

```cmake
find_package(tomlplusplus REQUIRED)
target_link_libraries(my_target PRIVATE tomlplusplus::tomlplusplus)
```

### CMake Options

```cmake
option(BUILD_EXAMPLES "Build examples tree." OFF)
option(BUILD_FUZZER "Build fuzzer." OFF)
option(TOMLPLUSPLUS_BUILD_MODULES "Build C++ modules support" OFF)
```

### CMake Install

When `tomlplusplus_INSTALL` is true, install rules are included from `cmake/install-rules.cmake`.

---

## Visual Studio

The repository includes Visual Studio project files:
- `toml++.sln` — Solution file
- `toml++.vcxproj` — Main project file
- `toml++.vcxproj.filters` — Filter definition
- `toml++.props` — Property sheet
- `toml++.natvis` — Natvis debugger visualizer for TOML node types

Individual examples also have `.vcxproj` files:
- `examples/simple_parser.vcxproj`
- `examples/toml_to_json_transcoder.vcxproj`
- `examples/toml_generator.vcxproj`
- `examples/error_printer.vcxproj`
- `examples/parse_benchmark.vcxproj`

---

## Package Managers

### Vcpkg

```bash
vcpkg install tomlplusplus
```

### Conan

In your `conanfile.txt`:
```
[requires]
tomlplusplus/3.4.0
```

### DDS

In your `package.json5`:
```json5
depends: [
    'tomlpp^3.4.0',
]
```

### tipi.build

In `.tipi/deps`:
```json
{
    "marzer/tomlplusplus": {}
}
```

---

## Compiler Requirements

### Minimum Versions

| Compiler | Minimum Version |
|----------|----------------|
| GCC | 8+ |
| Clang | 8+ |
| Apple Clang | Xcode 10+ |
| MSVC | VS2019 (19.20+) |
| Intel C++ | ICC 19+, ICL 19+ |

### Required Standard

C++17 is required. The preprocessor enforces this:

```cpp
#if TOML_CPP < 17
#error toml++ requires C++17 or higher.
#endif
```

### Compiler Feature Detection

The library detects compiler capabilities:

```cpp
// TOML_CPP — detected C++ standard version (11, 14, 17, 20, 23, 26, 29)
// TOML_GCC — GCC major version (0 if not GCC)
// TOML_CLANG — Clang major version (0 if not Clang)
// TOML_MSVC — MSVC version (0 if not MSVC)
// TOML_ICC — Intel compiler detection
// TOML_NVCC — NVIDIA CUDA compiler detection
// TOML_HAS_CHAR8 — char8_t available
// TOML_HAS_EXCEPTIONS — exceptions enabled
```

---

## Configuration Macros Reference

Define these **before** including `<toml++/toml.hpp>`:

### Core Configuration

```cpp
// Library mode
#define TOML_HEADER_ONLY 1        // 1 = header-only (default), 0 = compiled

// Feature toggles
#define TOML_ENABLE_PARSER 1      // 1 = include parser (default), 0 = no parser
#define TOML_ENABLE_FORMATTERS 1  // 1 = include formatters (default), 0 = no formatters

// Exception handling
// Auto-detected from compiler settings. Override with:
#define TOML_EXCEPTIONS 1         // or 0

// Unreleased TOML features
#define TOML_UNRELEASED_FEATURES 0  // 1 = enable upcoming TOML spec features
```

### Platform Configuration

```cpp
// Windows wide-string support (auto-detected on Windows)
#define TOML_ENABLE_WINDOWS_COMPAT 1

// Custom optional type
#define TOML_OPTIONAL_TYPE std::optional  // or your custom type

// Disable environment checks
#define TOML_DISABLE_ENVIRONMENT_CHECKS
```

### Example: Minimal Parse-Only Build

```cpp
#define TOML_ENABLE_FORMATTERS 0  // Don't need serialization
#include <toml++/toml.hpp>
```

### Example: Serialize-Only Build

```cpp
#define TOML_ENABLE_PARSER 0  // Don't need parsing
#include <toml++/toml.hpp>
```

---

## Build Troubleshooting

### Common Issues

**"toml++ requires C++17 or higher"**
Ensure your compiler is invoked with `-std=c++17` (or later) or the equivalent flag.

**Large object files on MSVC**
Use `/bigobj` flag (the Meson build adds this automatically).

**Long compile times**
Switch to compiled library mode (`TOML_HEADER_ONLY=0` + compile `src/toml.cpp`).

**ODR violations when mixing settings**
Ensure all translation units use the same values for `TOML_EXCEPTIONS`, `TOML_ENABLE_PARSER`, etc. The ABI namespace system catches some mismatches at link time, but not all.

**`char8_t` errors on older compilers**
Add `-fchar8_t` flag if your compiler supports it, or compile with C++20 mode.

**RTTI disabled**
toml++ does not require RTTI. It uses virtual dispatch, not `dynamic_cast` or `typeid`.

**Exceptions disabled**
Set `TOML_EXCEPTIONS=0` or use `-fno-exceptions`. The API adapts: `parse()` returns `parse_result` instead of throwing.

---

## Related Documentation

- [overview.md](overview.md) — Library feature list
- [basic-usage.md](basic-usage.md) — Getting started with parsing and serialization
- [testing.md](testing.md) — Running the test suite
