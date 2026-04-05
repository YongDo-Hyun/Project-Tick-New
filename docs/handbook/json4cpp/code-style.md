# json4cpp — Code Style & Conventions

## Source Organisation

### Directory Layout

```
json4cpp/
├── include/nlohmann/               # Multi-header installation
│   ├── json.hpp                     # Main header (includes everything)
│   ├── json_fwd.hpp                 # Forward declarations only
│   ├── adl_serializer.hpp           # ADL-based serializer
│   ├── byte_container_with_subtype.hpp
│   ├── ordered_map.hpp              # Insertion-order map
│   └── detail/                      # Internal implementation
│       ├── exceptions.hpp           # Exception hierarchy
│       ├── hash.hpp                 # std::hash specialization
│       ├── json_pointer.hpp         # RFC 6901 implementation
│       ├── json_ref.hpp             # Internal reference wrapper
│       ├── macro_scope.hpp          # Macro definitions
│       ├── macro_unscope.hpp        # Macro undefinitions
│       ├── string_concat.hpp        # String concatenation helper
│       ├── string_escape.hpp        # String escaping utilities
│       ├── value_t.hpp              # value_t enum
│       ├── abi_macros.hpp           # ABI versioning macros
│       ├── conversions/             # Type conversion traits
│       ├── input/                   # Parsing pipeline
│       ├── iterators/               # Iterator implementations
│       ├── meta/                    # Type traits & SFINAE
│       └── output/                  # Serialization pipeline
├── single_include/nlohmann/         # Single-header (amalgamated)
│   └── json.hpp                     # Complete library in one file
├── tests/                           # Test suite (doctest)
│   ├── CMakeLists.txt
│   └── src/
│       └── unit-*.cpp               # One file per feature area
└── CMakeLists.txt                   # Build configuration
```

### Public vs. Internal API

- `include/nlohmann/*.hpp` — public API, included by users
- `include/nlohmann/detail/` — internal, not for direct inclusion
- `single_include/` — generated amalgamation, mirrors the public API

Users should only include `<nlohmann/json.hpp>` or
`<nlohmann/json_fwd.hpp>`.

## Naming Conventions

### Types

- Template parameters: `PascalCase` — `BasicJsonType`, `ObjectType`,
  `InputAdapterType`
- Type aliases: `snake_case` — `value_t`, `object_t`, `string_t`,
  `number_integer_t`
- Internal classes: `snake_case` — `iter_impl`, `binary_reader`,
  `json_sax_dom_parser`

### Functions and Methods

- All functions: `snake_case` — `parse()`, `dump()`, `push_back()`,
  `is_null()`, `get_to()`, `merge_patch()`
- Private methods: `snake_case` — `set_parent()`, `assert_invariant()`

### Variables

- Member variables: `m_` prefix — `m_type`, `m_value`, `m_parent`
- Local variables: `snake_case` — `reference_tokens`, `token_buffer`

### Macros

- All macros: `SCREAMING_SNAKE_CASE` with project prefix
- Public macros: `NLOHMANN_` prefix or `JSON_` prefix
  - `NLOHMANN_DEFINE_TYPE_INTRUSIVE`
  - `NLOHMANN_JSON_SERIALIZE_ENUM`
  - `JSON_DIAGNOSTICS`
  - `JSON_USE_IMPLICIT_CONVERSIONS`
- Internal macros: `NLOHMANN_JSON_` prefix for implementation detail macros
- All macros are undefined by `macro_unscope.hpp` to avoid pollution

### Namespaces

```cpp
namespace nlohmann {
    // Public API: basic_json, json, ordered_json, json_pointer, ...
    namespace detail {
        // Internal implementation
    }
    namespace literals {
        namespace json_literals {
            // _json, _json_pointer UDLs
        }
    }
}
```

The `NLOHMANN_JSON_NAMESPACE_BEGIN` / `NLOHMANN_JSON_NAMESPACE_END` macros
handle optional ABI versioning via inline namespaces.

## Template Style

### SFINAE Guards

The library uses SFINAE extensively to constrain overloads:

```cpp
template<typename BasicJsonType, typename T,
         enable_if_t<is_compatible_type<BasicJsonType, T>::value, int> = 0>
void to_json(BasicJsonType& j, T&& val);
```

The `enable_if_t<..., int> = 0` pattern is used throughout instead of
`enable_if_t<..., void>` or return-type SFINAE.

### Tag Dispatch

Priority tags resolve overload ambiguity:

```cpp
template<unsigned N> struct priority_tag : priority_tag<N - 1> {};
template<> struct priority_tag<0> {};
```

Higher-numbered tags are tried first (since they inherit from lower ones).

### `static_assert` Guards

Critical type requirements use `static_assert` with readable messages:

```cpp
static_assert(std::is_default_constructible<T>::value,
              "T must be default constructible");
```

## Header Guards

Each header uses `#ifndef` guards following the pattern:

```cpp
#ifndef INCLUDE_NLOHMANN_JSON_HPP_
#define INCLUDE_NLOHMANN_JSON_HPP_
// ...
#endif  // INCLUDE_NLOHMANN_JSON_HPP_
```

Detail headers follow `INCLUDE_NLOHMANN_JSON_DETAIL_*` naming.

## Code Documentation

### Doxygen-Style Comments

Public API methods use `///` or `/** */` with standard Doxygen tags:

```cpp
/// @brief parse a JSON value from a string
/// @param[in] i the input to parse
/// @param[in] cb a callback function (default: none)
/// @param[in] allow_exceptions whether exceptions should be thrown
/// @return the parsed JSON value
static basic_json parse(InputType&& i, ...);
```

### `@sa` Cross References

Related methods are linked with `@sa`:

```cpp
/// @sa dump() for serialization
/// @sa operator>> for stream parsing
```

### `@throw` Documentation

Exception-throwing methods document which exceptions they throw:

```cpp
/// @throw parse_error.101 if unexpected token
/// @throw parse_error.102 if invalid unicode escape
```

## Error Handling Style

- Public API methods that can fail throw typed exceptions from the
  hierarchy (`parse_error`, `type_error`, `out_of_range`,
  `invalid_iterator`, `other_error`)
- Each exception has a unique numeric ID for programmatic handling
- Error messages follow the format:
  `[json.exception.<type>.<id>] <description>`
- Internal assertions use `JSON_ASSERT(condition)` which maps to
  `assert()` by default

## Compatibility

### C++ Standard

- Minimum: C++11
- Optional features with C++14: heterogeneous lookup
  (`std::less<>`)
- Optional features with C++17: `std::string_view`, `std::optional`,
  `std::variant`, `std::filesystem::path`, structured bindings,
  `if constexpr`
- Optional features with C++20: modules, `operator<=>`

### Compiler Notes

Tested compilers include GCC ≥ 4.8, Clang ≥ 3.4, MSVC ≥ 2015, Intel
C++, and various others. Compiler-specific workarounds are guarded with
preprocessor conditionals.
