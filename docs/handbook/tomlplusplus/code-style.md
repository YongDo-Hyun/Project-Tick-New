# toml++ — Code Style

## Overview

This document describes the code conventions and formatting rules used in the toml++ project, derived from the `.clang-format` configuration and source code patterns.

---

## Formatting Rules (`.clang-format`)

The project uses clang-format with these key settings:

### Indentation

- **IndentWidth**: 4 (tabs are used, tab width 4)
- **UseTab**: `ForContinuationAndIndentation`
- **TabWidth**: 4
- **ContinuationIndentWidth**: 4
- **ConstructorInitializerIndentWidth**: 4
- **AccessModifierOffset**: -4 (access specifiers at class indent level)
- **IndentCaseLabels**: true
- **NamespaceIndentation**: All

### Braces

- **BreakBeforeBraces**: Allman style
  - Functions, classes, structs, enums, namespaces, control statements — all open brace on new line:

```cpp
namespace toml
{
    class node
    {
      public:
        void method()
        {
            if (condition)
            {
                // ...
            }
        }
    };
}
```

### Alignment

- **AlignConsecutiveAssignments**: true
- **AlignConsecutiveDeclarations**: true
- **AlignTrailingComments**: true
- **AlignOperands**: true
- **AlignAfterOpenBracket**: Align

### Line Length

- **ColumnLimit**: 120

### Other Settings

- **AllowShortFunctionsOnASingleLine**: Empty (empty functions on one line)
- **AllowShortIfStatementsOnASingleLine**: Never
- **AllowShortLoopsOnASingleLine**: false
- **AlwaysBreakTemplateDeclarations**: Yes
- **BinPackArguments**: false
- **BinPackParameters**: false
- **PointerAlignment**: Left (`int* ptr`, not `int *ptr`)
- **SpaceAfterTemplateKeyword**: true
- **SortIncludes**: false (manual include ordering)

---

## Naming Conventions

### Macros

All macros use the `TOML_` prefix with `UPPER_SNAKE_CASE`:

```cpp
TOML_HEADER_ONLY
TOML_EXCEPTIONS
TOML_ENABLE_PARSER
TOML_ENABLE_FORMATTERS
TOML_ENABLE_WINDOWS_COMPAT
TOML_UNRELEASED_FEATURES
TOML_LIB_MAJOR
TOML_NAMESPACE_START
TOML_NAMESPACE_END
TOML_EXPORTED_CLASS
TOML_EXPORTED_MEMBER_FUNCTION
TOML_EXPORTED_STATIC_FUNCTION
TOML_EXPORTED_FREE_FUNCTION
```

### Namespaces

- Public API: `toml` namespace (aliased from a versioned namespace `toml::vN`)
- Internal implementation: `toml::impl` (aka `toml::vN::impl`)
- Macro-managed namespace boundaries:

```cpp
TOML_NAMESPACE_START  // opens toml::v3
{
    // public API
}
TOML_NAMESPACE_END    // closes

TOML_IMPL_NAMESPACE_START  // opens toml::v3::impl
{
    // internal details
}
TOML_IMPL_NAMESPACE_END
```

### Types and Classes

- `snake_case` for all types: `node`, `table`, `array`, `value`, `path`, `path_component`, `parse_result`, `parse_error`, `source_region`, `source_position`, `date_time`, `time_offset`, `node_view`, `key`
- Template parameters: `PascalCase` (`ValueType`, `IsConst`, `ViewedType`, `ElemType`)

### Member Variables

- Private members use trailing underscore: `val_`, `flags_`, `elems_`, `map_`, `inline_`, `source_`, `components_`
- No prefix for public struct fields: `year`, `month`, `day`, `line`, `column`, `begin`, `end`, `path`

### Methods

- `snake_case`: `is_table()`, `as_array()`, `value_or()`, `push_back()`, `emplace_back()`, `is_homogeneous()`, `for_each()`, `parse_file()`, `at_path()`

### Enums

- `snake_case` enum type names: `node_type`, `value_flags`, `format_flags`, `path_component_type`
- `snake_case` enum values: `node_type::string`, `value_flags::format_as_hexadecimal`, `format_flags::indent_sub_tables`

---

## Header Organization

### File Pairs

Most features have a `.hpp` declaration header and a `.inl` implementation file:

```
node.hpp / node.inl
table.hpp / table.inl
array.hpp / array.inl
parser.hpp / parser.inl
formatter.hpp / formatter.inl
```

### Include Guards

Headers use `#pragma once` (no traditional include guards).

### Header Structure

Typical header layout:

```cpp
// license header comment
#pragma once

#include "preprocessor.hpp"     // macros and config
#include "forward_declarations.hpp"  // forward declarations
// ... other includes

// Header-only mode guard
#if defined(TOML_IMPLEMENTATION) || !TOML_HEADER_ONLY

TOML_NAMESPACE_START
{
    // declarations / implementations
}
TOML_NAMESPACE_END

#endif  // TOML_IMPLEMENTATION
```

### Export Annotations

Exported symbols use macros for DLL visibility:

```cpp
TOML_EXPORTED_CLASS table : public node
{
    TOML_EXPORTED_MEMBER_FUNCTION void clear() noexcept;
    TOML_EXPORTED_STATIC_FUNCTION static table parse(...);
};

TOML_EXPORTED_FREE_FUNCTION parse_result parse(std::string_view);
```

---

## Preprocessor Conventions

### Compiler Detection

```cpp
TOML_GCC        // GCC
TOML_CLANG      // Clang
TOML_MSVC       // MSVC
TOML_ICC        // Intel C++
TOML_ICC_CL     // Intel C++ (MSVC frontend)
```

### Feature Detection

```cpp
TOML_HAS_CHAR8            // char8_t available
TOML_HAS_CUSTOM_OPTIONAL_TYPE  // user-provided optional
TOML_INT_CHARCONV          // charconv for integers
TOML_FLOAT_CHARCONV        // charconv for floats
```

### Warning Management

Extensive `#pragma` blocks suppress known-benign warnings per compiler:

```cpp
TOML_PUSH_WARNINGS
TOML_DISABLE_WARNINGS
// ... code ...
TOML_POP_WARNINGS

TOML_DISABLE_ARITHMETIC_WARNINGS
TOML_DISABLE_SPAM_WARNINGS
```

---

## Conditional Compilation Patterns

Major features are conditionally compiled:

```cpp
#if TOML_ENABLE_PARSER
    // parser code
#endif

#if TOML_ENABLE_FORMATTERS
    // formatter code
#endif

#if TOML_ENABLE_WINDOWS_COMPAT
    // wchar_t / wstring overloads
#endif

#if TOML_EXCEPTIONS
    // exception-based error handling
#else
    // return-code error handling
#endif
```

---

## Documentation Conventions

- Source comments use `//` style (not `/* */`)
- Doxygen is used for API documentation (the public `toml.hpp` single-header has `///` comments)
- Internal implementation headers have minimal comments — the code is expected to be self-documenting

---

## Build System Conventions

- Primary build: **Meson** (`meson.build`, `meson_options.txt`)
- Secondary: **CMake** (`CMakeLists.txt`)
- All configuration macros can be set via build system options or via `#define` before including the header
- Meson option names mirror the macro names: `is_header_only` → `TOML_HEADER_ONLY`

---

## Related Documentation

- [architecture.md](architecture.md) — Project structure and design
- [building.md](building.md) — Build system details
- [testing.md](testing.md) — Testing conventions
