# toml++ (tomlplusplus) — Overview

## What Is toml++?

toml++ is a header-only TOML v1.0 parser, serializer, and data model library for C++17 and later. It is authored by Mark Gillard and published under the MIT license. The library version as of this documentation is **3.4.0**, implementing TOML language specification version **1.0.0**.

The library lives in the `toml` namespace and provides a complete object model for TOML documents: tables, arrays, and typed values. It can parse TOML from strings, streams, and files; manipulate the resulting tree programmatically; and serialize back to TOML, JSON, or YAML.

Repository: [https://github.com/marzer/tomlplusplus](https://github.com/marzer/tomlplusplus)

---

## What Is TOML?

TOML stands for **Tom's Obvious Minimal Language**. It is a configuration file format designed to be easy to read, unambiguous, and map cleanly to a hash table (dictionary). A TOML document is fundamentally a collection of key-value pairs organized into tables.

### TOML Data Types

TOML defines the following native data types:

| TOML Type       | C++ Representation in toml++ | `node_type` Enum              |
|-----------------|------------------------------|-------------------------------|
| String          | `std::string`                | `node_type::string`           |
| Integer         | `int64_t`                    | `node_type::integer`          |
| Float           | `double`                     | `node_type::floating_point`   |
| Boolean         | `bool`                       | `node_type::boolean`          |
| Local Date      | `toml::date`                 | `node_type::date`             |
| Local Time      | `toml::time`                 | `node_type::time`             |
| Offset Date-Time| `toml::date_time`            | `node_type::date_time`        |
| Array           | `toml::array`                | `node_type::array`            |
| Table           | `toml::table`                | `node_type::table`            |

### Example TOML Document

```toml
# This is a TOML document

title = "TOML Example"

[owner]
name = "Tom Preston-Werner"
dob = 1979-05-27T07:32:00-08:00

[database]
enabled = true
ports = [ 8000, 8001, 8002 ]
data = [ ["delta", "phi"], [3.14] ]
temp_targets = { cpu = 79.5, case = 72.0 }

[servers]

[servers.alpha]
ip = "10.0.0.1"
role = "frontend"

[servers.beta]
ip = "10.0.0.2"
role = "backend"
```

---

## C++17 Features Used

toml++ requires C++17 as its minimum standard. The version detection logic in `preprocessor.hpp` checks `__cplusplus` and `_MSVC_LANG`, rejecting anything below C++17:

```cpp
#if TOML_CPP < 17
#error toml++ requires C++17 or higher.
#endif
```

Key C++17 features utilized throughout the library:

### `std::string_view`
Used pervasively for zero-copy string references in parsing, key lookups, path parsing, and formatting. The parser accepts `std::string_view` for document content and source paths.

### `std::optional<T>`
Returned by value retrieval functions like `node::value<T>()` and `node::value_exact<T>()`. The library also supports a custom optional type via `TOML_HAS_CUSTOM_OPTIONAL_TYPE`.

### `if constexpr`
Used extensively in template code for compile-time type dispatch. For example, `node::is<T>()` and `node::as<T>()` use `if constexpr` chains to dispatch to the correct type check or cast:

```cpp
template <typename T>
bool is() const noexcept
{
    using type = impl::remove_cvref<impl::unwrap_node<T>>;
    if constexpr (std::is_same_v<type, table>)
        return is_table();
    else if constexpr (std::is_same_v<type, array>)
        return is_array();
    else if constexpr (std::is_same_v<type, std::string>)
        return is_string();
    // ...
}
```

### Structured Bindings
Table iteration supports structured bindings:

```cpp
for (auto&& [key, value] : my_table)
{
    std::cout << key << " = " << value << "\n";
}
```

### Fold Expressions
Used in template parameter packs throughout the implementation, such as in `impl::all_integral<>` constraints for date/time constructors.

### `std::variant` / `std::any` Awareness
While not directly depending on `std::variant`, the library includes `std_variant.hpp` for platforms that need it. The node hierarchy itself is polymorphic (virtual dispatch), not variant-based.

### Inline Variables
Used for constants like `impl::node_type_friendly_names[]` and `impl::control_char_escapes[]`, which are declared `inline constexpr` in header files.

### Class Template Argument Deduction (CTAD)
`toml::value` supports CTAD for constructing values from native types without explicitly specifying the template parameter.

---

## C++20 Feature Support

When compiled under C++20 or later, toml++ optionally supports:

- **`char8_t` strings**: When `TOML_HAS_CHAR8` is true, the parser accepts `std::u8string_view` and `std::u8string` inputs. File paths can also be `char8_t`-based.
- **C++20 Modules**: The library ships with experimental module support via `import tomlplusplus;`. Enabled by setting `TOMLPLUSPLUS_BUILD_MODULES=ON` in CMake (requires CMake 3.28+).

---

## Complete Feature List

### Parsing
- Parse TOML from `std::string_view`, `std::istream`, or files (`toml::parse()`, `toml::parse_file()`)
- Full TOML v1.0.0 conformance — passes all tests in the [toml-test](https://github.com/toml-lang/toml-test) suite
- Optional support for unreleased TOML features (e.g., unicode bare keys from toml/pull/891)
- Proper UTF-8 handling including BOM detection and skipping
- Detailed error reporting with source positions (`toml::parse_error`, `toml::source_region`)
- Works with or without exceptions (`TOML_EXCEPTIONS` macro)
- Support for `char8_t` strings (C++20)
- Windows wide string compatibility (`TOML_ENABLE_WINDOWS_COMPAT`)

### Data Model
- Complete node type hierarchy: `toml::node` (abstract base) → `toml::table`, `toml::array`, `toml::value<T>`
- `toml::node_view<T>` for safe, optional-like node access with chained subscript operators
- `toml::key` class preserving source location information for parsed keys
- Type-safe value access: `node::value<T>()`, `node::value_exact<T>()`, `node::value_or(default)`
- Template `as<T>()` for casting nodes to concrete types
- `is<T>()` family for type checking
- Visitor pattern via `node::visit()` and `for_each()`
- Homogeneity checking with `is_homogeneous()`

### Manipulation
- Construct tables and arrays programmatically using initializer lists
- `table::insert()`, `table::insert_or_assign()`, `table::emplace()`
- `array::push_back()`, `array::emplace_back()`, `array::insert()`
- `table::erase()`, `array::erase()`
- Deep copy via copy constructors
- All containers are movable

### Navigation
- `operator[]` subscript chaining: `tbl["section"]["key"]`
- `toml::at_path()` free function for dot-separated path lookup: `at_path(tbl, "section.key[0]")`
- `toml::path` class for programmatic path construction and manipulation
- `path::parent_path()`, `path::leaf()`, `path::subpath()`
- Path components are either keys (`std::string`) or array indices (`size_t`)

### Serialization
- `toml::toml_formatter` — serialize as valid TOML (default when streaming nodes)
- `toml::json_formatter` — serialize as JSON
- `toml::yaml_formatter` — serialize as YAML
- All formatters support `format_flags` for fine-grained output control
- Format flags include: `indent_array_elements`, `indent_sub_tables`, `allow_literal_strings`, `allow_multi_line_strings`, `allow_unicode_strings`, `allow_real_tabs_in_strings`, `allow_binary_integers`, `allow_octal_integers`, `allow_hexadecimal_integers`, `quote_dates_and_times`, `quote_infinities_and_nans`, `terse_key_value_pairs`, `force_multiline_arrays`
- Inline table detection (`table::is_inline()`)
- Value flags for controlling integer format representation (`value_flags::format_as_binary`, `format_as_octal`, `format_as_hexadecimal`)

### Build Modes
- **Header-only** (default): just `#include <toml++/toml.hpp>`
- **Single-header**: drop `toml.hpp` (root-level amalgamated file) into your project
- **Compiled library**: define `TOML_HEADER_ONLY=0` and compile `src/toml.cpp`
- **C++20 Modules**: `import tomlplusplus;`

### Build Systems
- Meson (primary, with full option support)
- CMake (interface library target `tomlplusplus::tomlplusplus`)
- Visual Studio solution files (`.sln`, `.vcxproj`)
- Package managers: Conan, Vcpkg, DDS, tipi.build

### Compiler Support
- GCC 8+
- Clang 8+ (including Apple Clang)
- MSVC (VS2019+)
- Intel C++ Compiler (ICC/ICL)
- NVIDIA CUDA Compiler (NVCC) with workarounds

### Platform Support
- x86, x64, ARM architectures
- Windows, Linux, macOS
- Does not require RTTI
- Works with or without exceptions

---

## Version Information

Version constants are defined in `include/toml++/impl/version.hpp`:

```cpp
#define TOML_LIB_MAJOR 3
#define TOML_LIB_MINOR 4
#define TOML_LIB_PATCH 0

#define TOML_LANG_MAJOR 1
#define TOML_LANG_MINOR 0
#define TOML_LANG_PATCH 0
```

- `TOML_LIB_MAJOR/MINOR/PATCH` — the library version (3.4.0)
- `TOML_LANG_MAJOR/MINOR/PATCH` — the TOML specification version implemented (1.0.0)

---

## Configuration Macros

toml++ is heavily configurable via preprocessor macros. Key ones include:

| Macro | Default | Description |
|-------|---------|-------------|
| `TOML_HEADER_ONLY` | `1` | When `1`, the library is header-only. Set to `0` for compiled mode. |
| `TOML_EXCEPTIONS` | auto-detected | Whether to use exceptions. Auto-detected from compiler settings. |
| `TOML_ENABLE_PARSER` | `1` | Set to `0` to disable the parser entirely (serialization only). |
| `TOML_ENABLE_FORMATTERS` | `1` | Set to `0` to disable all formatters. |
| `TOML_ENABLE_WINDOWS_COMPAT` | `1` on Windows | Enables `std::wstring` overloads for Windows. |
| `TOML_UNRELEASED_FEATURES` | `0` | Enable support for unreleased TOML spec features. |
| `TOML_HAS_CUSTOM_OPTIONAL_TYPE` | `0` | Define with a custom optional type to use instead of `std::optional`. |
| `TOML_DISABLE_ENVIRONMENT_CHECKS` | undefined | Define to skip compile-time environment validation. |

### Environment Ground-Truths

The library validates its environment at compile time (unless `TOML_DISABLE_ENVIRONMENT_CHECKS` is defined):

```cpp
static_assert(CHAR_BIT == 8, TOML_ENV_MESSAGE);
static_assert('A' == 65, TOML_ENV_MESSAGE);  // ASCII
static_assert(sizeof(double) == 8, TOML_ENV_MESSAGE);
static_assert(std::numeric_limits<double>::is_iec559, TOML_ENV_MESSAGE);  // IEEE 754
```

These ensure the library operates on platforms with 8-bit bytes, ASCII character encoding, and IEEE 754 double-precision floats.

---

## Namespace Organization

The library uses a layered namespace structure:

- **`toml`** — The root namespace containing all public API types: `table`, `array`, `value<T>`, `node`, `node_view`, `key`, `path`, `date`, `time`, `date_time`, `source_position`, `source_region`, `parse_error`, `parse_result`, etc.
- **`toml::impl`** (internal) — Implementation details not part of the public API. Contains the parser, formatter base class, iterator implementations, and type trait utilities.
- **ABI namespaces** — Conditional inline namespaces (e.g., `ex`/`noex` for exception mode, `custopt`/`stdopt` for optional type) ensure ABI compatibility when linking translation units compiled with different settings.

---

## Type Traits and Concepts

The `toml` namespace provides several type trait utilities:

```cpp
toml::is_value<T>      // true for native value types (std::string, int64_t, double, bool, date, time, date_time)
toml::is_container<T>  // true for table and array
toml::is_string<T>     // true if T is a toml::value<std::string> or node_view thereof
toml::is_integer<T>    // true if T is a toml::value<int64_t> or node_view thereof
toml::is_floating_point<T>
toml::is_number<T>
toml::is_boolean<T>
toml::is_date<T>
toml::is_time<T>
toml::is_date_time<T>
toml::is_table<T>
toml::is_array<T>
```

These are usable in `if constexpr` and `static_assert` contexts, making generic TOML-processing code straightforward.

---

## The `node_type` Enumeration

Defined in the forward declarations, `node_type` identifies the kind of a TOML node:

```cpp
enum class node_type : uint8_t
{
    none,            // Not a valid node type (used as nil sentinel)
    table,           // toml::table
    array,           // toml::array
    string,          // toml::value<std::string>
    integer,         // toml::value<int64_t>
    floating_point,  // toml::value<double>
    boolean,         // toml::value<bool>
    date,            // toml::value<toml::date>
    time,            // toml::value<toml::time>
    date_time        // toml::value<toml::date_time>
};
```

Friendly display names are available via `impl::node_type_friendly_names[]`:
`"none"`, `"table"`, `"array"`, `"string"`, `"integer"`, `"floating-point"`, `"boolean"`, `"date"`, `"time"`, `"date-time"`.

---

## The `value_flags` Enumeration

Controls how integer values are formatted during serialization:

```cpp
enum class value_flags : uint16_t
{
    none                = 0,
    format_as_binary    = 1,   // 0b...
    format_as_octal     = 2,   // 0o...
    format_as_hexadecimal = 3  // 0x...
};
```

The sentinel value `preserve_source_value_flags` tells the library to keep whatever format the parser originally detected.

---

## The `format_flags` Enumeration

Controls formatter output behavior. It is a bitmask enum:

```cpp
enum class format_flags : uint64_t
{
    none                          = 0,
    quote_dates_and_times         = 1,
    quote_infinities_and_nans     = 2,
    allow_literal_strings         = 4,
    allow_multi_line_strings      = 8,
    allow_real_tabs_in_strings    = 16,
    allow_unicode_strings         = 32,
    allow_binary_integers         = 64,
    allow_octal_integers          = 128,
    allow_hexadecimal_integers    = 256,
    indent_sub_tables             = 512,
    indent_array_elements         = 1024,
    indentation                   = indent_sub_tables | indent_array_elements,
    terse_key_value_pairs         = 2048,
    force_multiline_arrays        = 4096
};
```

Each formatter has its own `default_flags` static constant and a set of mandatory/ignored flags defined in `formatter_constants`.

---

## Minimal Usage Example

```cpp
#include <toml++/toml.hpp>
#include <iostream>

int main()
{
    // Parse a TOML string
    auto config = toml::parse(R"(
        [server]
        host = "localhost"
        port = 8080
        debug = false
    )");

    // Read values
    std::string_view host = config["server"]["host"].value_or("0.0.0.0"sv);
    int64_t port = config["server"]["port"].value_or(80);
    bool debug = config["server"]["debug"].value_or(true);

    std::cout << "Host: " << host << "\n";
    std::cout << "Port: " << port << "\n";
    std::cout << "Debug: " << std::boolalpha << debug << "\n";

    // Serialize back to TOML
    std::cout << "\n" << config << "\n";

    // Serialize as JSON
    std::cout << toml::json_formatter{ config } << "\n";

    return 0;
}
```

---

## File Organization

```
tomlplusplus/
├── toml.hpp                    # Single-header amalgamation (drop-in)
├── include/
│   └── toml++/
│       ├── toml.hpp            # Main include (includes all impl headers)
│       ├── toml.h              # C-compatible header alias
│       └── impl/
│           ├── forward_declarations.hpp  # All forward decls, type aliases
│           ├── preprocessor.hpp          # Compiler/platform detection, macros
│           ├── version.hpp               # Version constants
│           ├── node.hpp                  # toml::node base class
│           ├── node.inl                  # node method implementations
│           ├── node_view.hpp             # toml::node_view<T>
│           ├── table.hpp                 # toml::table
│           ├── table.inl                 # table method implementations
│           ├── array.hpp                 # toml::array
│           ├── array.inl                 # array method implementations
│           ├── value.hpp                 # toml::value<T>
│           ├── key.hpp                   # toml::key
│           ├── date_time.hpp             # toml::date, toml::time, toml::date_time
│           ├── source_region.hpp         # source_position, source_region
│           ├── parser.hpp                # toml::parse(), toml::parse_file()
│           ├── parser.inl                # Parser implementation
│           ├── parse_error.hpp           # toml::parse_error
│           ├── parse_result.hpp          # toml::parse_result (no-exceptions mode)
│           ├── path.hpp                  # toml::path, toml::path_component
│           ├── path.inl                  # Path implementation
│           ├── at_path.hpp               # toml::at_path() free function
│           ├── at_path.inl               # at_path implementation
│           ├── formatter.hpp             # impl::formatter base class
│           ├── formatter.inl             # formatter base implementation
│           ├── toml_formatter.hpp        # toml::toml_formatter
│           ├── toml_formatter.inl        # TOML formatter implementation
│           ├── json_formatter.hpp        # toml::json_formatter
│           ├── json_formatter.inl        # JSON formatter implementation
│           ├── yaml_formatter.hpp        # toml::yaml_formatter
│           ├── yaml_formatter.inl        # YAML formatter implementation
│           ├── make_node.hpp             # impl::make_node() factory
│           ├── print_to_stream.hpp/.inl  # Stream output helpers
│           ├── unicode.hpp               # Unicode utilities
│           ├── unicode.inl               # UTF-8 decoder
│           ├── unicode_autogenerated.hpp # Auto-generated Unicode tables
│           └── std_*.hpp                 # Standard library includes
├── src/
│   └── toml.cpp                # Compiled-library translation unit
├── tests/                      # Catch2-based test suite
├── examples/                   # Example programs
├── tools/                      # Build/generation tools
├── meson.build                 # Primary build system
├── CMakeLists.txt              # CMake build system
└── toml-test/                  # toml-test conformance suite integration
```

---

## License

toml++ is licensed under the **MIT License**. See `LICENSE` in the repository root for the full text.

---

## Related Documentation

- [architecture.md](architecture.md) — Class hierarchy and internal design
- [building.md](building.md) — Build instructions and integration
- [basic-usage.md](basic-usage.md) — Common usage patterns
- [node-system.md](node-system.md) — The node type system in depth
- [tables.md](tables.md) — Working with toml::table
- [arrays.md](arrays.md) — Working with toml::array
- [values.md](values.md) — Working with toml::value<T>
- [parsing.md](parsing.md) — Parser internals and error handling
- [formatting.md](formatting.md) — Output formatters
- [path-system.md](path-system.md) — Path-based navigation
- [unicode-handling.md](unicode-handling.md) — UTF-8 and Unicode support
- [code-style.md](code-style.md) — Code conventions
- [testing.md](testing.md) — Test framework and conformance
