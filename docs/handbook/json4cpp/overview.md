# json4cpp — Overview

## What is json4cpp?

json4cpp is the Project-Tick vendored copy of **nlohmann/json** (version 3.12.0),
a header-only C++ library for working with JSON data. Created by Niels Lohmann,
it provides a first-class JSON type (`nlohmann::json`) that behaves like an STL
container and integrates seamlessly with modern C++ idioms.

The library is designed around one central class template:

```cpp
template<
    template<typename U, typename V, typename... Args> class ObjectType = std::map,
    template<typename U, typename... Args> class ArrayType = std::vector,
    class StringType = std::string,
    class BooleanType = bool,
    class NumberIntegerType = std::int64_t,
    class NumberUnsignedType = std::uint64_t,
    class NumberFloatType = double,
    template<typename U> class AllocatorType = std::allocator,
    template<typename T, typename SFINAE = void> class JSONSerializer = adl_serializer,
    class BinaryType = std::vector<std::uint8_t>,
    class CustomBaseClass = void
>
class basic_json;
```

The default specialization is the convenient type alias:

```cpp
using json = basic_json<>;
```

An insertion-order-preserving variant is also provided:

```cpp
using ordered_json = basic_json<nlohmann::ordered_map>;
```

## Key Features

### Header-Only Design

The entire library ships in a single header (`single_include/nlohmann/json.hpp`)
or as a multi-header tree rooted at `include/nlohmann/json.hpp`. No compilation
of library code is needed — just `#include` and use.

The multi-header layout, used when `JSON_MultipleHeaders` is ON in CMake,
breaks the implementation into focused files under `include/nlohmann/detail/`:

| Directory / File | Purpose |
|---|---|
| `detail/value_t.hpp` | `value_t` enumeration of JSON types |
| `detail/exceptions.hpp` | Exception hierarchy (`parse_error`, `type_error`, etc.) |
| `detail/json_pointer.hpp` | RFC 6901 JSON Pointer |
| `detail/input/lexer.hpp` | Tokenizer / lexical analyzer |
| `detail/input/parser.hpp` | Recursive-descent parser |
| `detail/input/json_sax.hpp` | SAX interface and DOM builders |
| `detail/input/binary_reader.hpp` | CBOR / MessagePack / UBJSON / BSON / BJData reader |
| `detail/input/input_adapters.hpp` | Input source abstraction (file, stream, string, iterators) |
| `detail/output/serializer.hpp` | JSON text serializer with UTF-8 validation |
| `detail/output/binary_writer.hpp` | Binary format writers |
| `detail/output/output_adapters.hpp` | Output sink abstraction |
| `detail/iterators/iter_impl.hpp` | Iterator implementation |
| `detail/iterators/iteration_proxy.hpp` | `items()` proxy for key-value iteration |
| `detail/conversions/from_json.hpp` | Default `from_json()` overloads |
| `detail/conversions/to_json.hpp` | Default `to_json()` overloads |
| `detail/macro_scope.hpp` | Configuration macros, `NLOHMANN_DEFINE_TYPE_*` |
| `detail/meta/type_traits.hpp` | SFINAE helpers and concept checks |

### Intuitive Syntax

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Create a JSON object
json j = {
    {"name", "Project-Tick"},
    {"version", 3},
    {"features", {"parsing", "serialization", "patch"}},
    {"active", true}
};

// Access values
std::string name = j["name"];
int version = j.at("version");

// Iterate
for (auto& [key, val] : j.items()) {
    std::cout << key << ": " << val << "\n";
}

// Serialize
std::string pretty = j.dump(4);
```

### STL Container Compatibility

`basic_json` models an STL container — it defines the standard type aliases
and fulfills the Container concept requirements:

```cpp
// Container type aliases defined by basic_json:
using value_type       = basic_json;
using reference        = value_type&;
using const_reference  = const value_type&;
using difference_type  = std::ptrdiff_t;
using size_type        = std::size_t;
using allocator_type   = AllocatorType<basic_json>;
using pointer          = typename std::allocator_traits<allocator_type>::pointer;
using const_pointer    = typename std::allocator_traits<allocator_type>::const_pointer;
using iterator         = iter_impl<basic_json>;
using const_iterator   = iter_impl<const basic_json>;
using reverse_iterator = json_reverse_iterator<typename basic_json::iterator>;
using const_reverse_iterator = json_reverse_iterator<typename basic_json::const_iterator>;
```

This means `basic_json` works with STL algorithms:

```cpp
json arr = {3, 1, 4, 1, 5};
std::sort(arr.begin(), arr.end());
auto it = std::find(arr.begin(), arr.end(), 4);
```

### Implicit Type Conversions

By default (`JSON_USE_IMPLICIT_CONVERSIONS=1`), values can be implicitly
converted to native C++ types:

```cpp
json j = 42;
int x = j;          // implicit conversion
std::string s = j;  // throws type_error::302 — type mismatch
```

This can be disabled at compile time with `-DJSON_ImplicitConversions=OFF`
(sets `JSON_USE_IMPLICIT_CONVERSIONS` to 0), requiring explicit `.get<T>()`
calls instead.

### Comprehensive JSON Value Types

Every JSON value type maps to a C++ type through the `value_t` enumeration
defined in `detail/value_t.hpp`:

| JSON Type | `value_t` Enumerator | C++ Storage Type | Default |
|---|---|---|---|
| Object | `value_t::object` | `object_t*` | `std::map<std::string, basic_json>` |
| Array | `value_t::array` | `array_t*` | `std::vector<basic_json>` |
| String | `value_t::string` | `string_t*` | `std::string` |
| Boolean | `value_t::boolean` | `boolean_t` | `bool` |
| Integer | `value_t::number_integer` | `number_integer_t` | `std::int64_t` |
| Unsigned | `value_t::number_unsigned` | `number_unsigned_t` | `std::uint64_t` |
| Float | `value_t::number_float` | `number_float_t` | `double` |
| Binary | `value_t::binary` | `binary_t*` | `byte_container_with_subtype<vector<uint8_t>>` |
| Null | `value_t::null` | (none) | — |
| Discarded | `value_t::discarded` | (none) | — |

Variable-length types (object, array, string, binary) are stored as heap
pointers to keep the `json_value` union at 8 bytes on 64-bit platforms.

### Binary Format Support

Beyond JSON text, the library supports round-trip conversion to and from
several binary serialization formats:

- **CBOR** (RFC 7049) — `to_cbor()` / `from_cbor()`
- **MessagePack** — `to_msgpack()` / `from_msgpack()`
- **UBJSON** — `to_ubjson()` / `from_ubjson()`
- **BSON** (MongoDB) — `to_bson()` / `from_bson()`
- **BJData** — `to_bjdata()` / `from_bjdata()`

### RFC Compliance

| Feature | Specification |
|---|---|
| JSON Pointer | RFC 6901 — navigating JSON documents with path syntax |
| JSON Patch | RFC 6902 — describing mutations as operation arrays |
| JSON Merge Patch | RFC 7396 — simplified document merging |

### SAX-Style Parsing

For memory-constrained scenarios or streaming, the SAX interface
(`json_sax<BasicJsonType>`) allows event-driven parsing without building
a DOM tree in memory.

### Custom Type Serialization

The ADL-based serializer architecture lets users define `to_json()` and
`from_json()` free functions for any user-defined type. Convenience macros
automate this:

- `NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, member1, member2, ...)`
- `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Type, member1, member2, ...)`
- `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Type, ...)`
- `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, ...)`
- `NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Type, ...)`
- `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(Type, ...)`
- `NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(Type, BaseType, ...)`

### User-Defined Literals

When `JSON_USE_GLOBAL_UDLS` is enabled (the default), two string literals
are available globally:

```cpp
auto j = R"({"key": "value"})"_json;
auto ptr = "/key"_json_pointer;
```

These are always available as `nlohmann::literals::json_literals::operator""_json`
and `operator""_json_pointer`.

## Version Information

The library provides compile-time version macros:

```cpp
NLOHMANN_JSON_VERSION_MAJOR  // 3
NLOHMANN_JSON_VERSION_MINOR  // 12
NLOHMANN_JSON_VERSION_PATCH  // 0
```

And a runtime introspection method:

```cpp
json meta = json::meta();
// Returns:
// {
//   "copyright": "(C) 2013-2026 Niels Lohmann",
//   "name": "JSON for Modern C++",
//   "url": "https://github.com/nlohmann/json",
//   "version": {"string": "3.12.0", "major": 3, "minor": 12, "patch": 0},
//   "compiler": {...},
//   "platform": "linux"
// }
```

## Compiler Support

The library requires C++11 at minimum. Higher standard modes unlock
additional features:

| Standard | Features Enabled |
|---|---|
| C++11 | Full library functionality |
| C++14 | `constexpr` support for `get<>()`, transparent comparators (`std::less<>`) |
| C++17 | `std::string_view` support, `std::any` integration, `if constexpr` |
| C++20 | Three-way comparison (`<=>` / `std::partial_ordering`), `std::format` |

Automatic detection uses `__cplusplus` (or `_MSVC_LANG` on MSVC) and defines:

- `JSON_HAS_CPP_11` — always 1
- `JSON_HAS_CPP_14` — C++14 or above
- `JSON_HAS_CPP_17` — C++17 or above
- `JSON_HAS_CPP_20` — C++20 or above

## Configuration Macros

The library's behavior is controlled by preprocessor macros, typically set
via CMake options:

| Macro | CMake Option | Default | Effect |
|---|---|---|---|
| `JSON_DIAGNOSTICS` | `JSON_Diagnostics` | `OFF` | Extended diagnostic messages with parent paths |
| `JSON_DIAGNOSTIC_POSITIONS` | `JSON_Diagnostic_Positions` | `OFF` | Track byte positions in parsed values |
| `JSON_USE_IMPLICIT_CONVERSIONS` | `JSON_ImplicitConversions` | `ON` | Allow implicit `operator ValueType()` |
| `JSON_DISABLE_ENUM_SERIALIZATION` | `JSON_DisableEnumSerialization` | `OFF` | Disable automatic enum-to-int conversion |
| `JSON_USE_GLOBAL_UDLS` | `JSON_GlobalUDLs` | `ON` | Place UDLs in global namespace |
| `JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON` | `JSON_LegacyDiscardedValueComparison` | `OFF` | Old comparison behavior for discarded values |
| `JSON_NO_IO` | — | (not set) | Disable `<iosfwd>` / stream operators |

## License

nlohmann/json is released under the **MIT License**.

```
SPDX-License-Identifier: MIT
SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
```

## Directory Structure in Project-Tick

```
json4cpp/
├── CMakeLists.txt          # Top-level build configuration
├── include/nlohmann/       # Multi-header source tree
│   ├── json.hpp            # Main header
│   ├── json_fwd.hpp        # Forward declarations
│   ├── adl_serializer.hpp  # ADL serializer
│   ├── byte_container_with_subtype.hpp
│   ├── ordered_map.hpp     # Insertion-order map
│   └── detail/             # Implementation details
├── single_include/nlohmann/
│   ├── json.hpp            # Amalgamated single header
│   └── json_fwd.hpp        # Forward declarations
├── tests/                  # Doctest-based test suite
├── docs/                   # Upstream documentation source
├── tools/                  # Code generation and maintenance scripts
├── cmake/                  # CMake modules and configs
├── BUILD.bazel             # Bazel build file
├── MODULE.bazel            # Bazel module definition
├── Package.swift           # Swift Package Manager support
├── meson.build             # Meson build file
└── Makefile                # Convenience Makefile
```

## Further Reading

The remaining handbook documents cover:

- **architecture.md** — Internal class hierarchy and template structure
- **building.md** — Integration methods, CMake support, compilation options
- **basic-usage.md** — Creating JSON values, accessing data, type system
- **value-types.md** — All JSON value types and their C++ representation
- **element-access.md** — `operator[]`, `at()`, `value()`, `find()`, `contains()`
- **iteration.md** — Iterators, range-for, `items()`, structured bindings
- **serialization.md** — `dump()`, `parse()`, stream I/O, `to_json`/`from_json`
- **binary-formats.md** — MessagePack, CBOR, BSON, UBJSON, BJData
- **json-pointer.md** — RFC 6901 JSON Pointer navigation
- **json-patch.md** — RFC 6902 JSON Patch and RFC 7396 Merge Patch
- **custom-types.md** — ADL serialization and `NLOHMANN_DEFINE_TYPE_*` macros
- **parsing-internals.md** — Lexer, parser, and SAX DOM builder internals
- **exception-handling.md** — Exception types, error IDs, when they are thrown
- **sax-interface.md** — SAX-style event-driven parsing
- **performance.md** — Performance characteristics and tuning
- **code-style.md** — Source code conventions
- **testing.md** — Test framework and running tests
