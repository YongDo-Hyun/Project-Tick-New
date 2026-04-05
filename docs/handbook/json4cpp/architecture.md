# json4cpp — Architecture

## Overview

The json4cpp library (nlohmann/json 3.12.0) is organized as a heavily
templatized, header-only C++ library. The architecture revolves around a single
class template, `basic_json`, whose template parameters allow customization of
every underlying storage type. This document describes the internal structure,
class hierarchy, memory layout, and key design patterns.

## The `basic_json` Class Template

### Template Declaration

The full template declaration in `include/nlohmann/json_fwd.hpp`:

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

Each parameter controls a specific aspect:

| Parameter | Purpose | Default |
|---|---|---|
| `ObjectType` | Map template for JSON objects | `std::map` |
| `ArrayType` | Sequential container for JSON arrays | `std::vector` |
| `StringType` | String type for keys and string values | `std::string` |
| `BooleanType` | Boolean storage | `bool` |
| `NumberIntegerType` | Signed integer type | `std::int64_t` |
| `NumberUnsignedType` | Unsigned integer type | `std::uint64_t` |
| `NumberFloatType` | Floating-point type | `double` |
| `AllocatorType` | Allocator template | `std::allocator` |
| `JSONSerializer` | Serializer template for custom types | `adl_serializer` |
| `BinaryType` | Container for binary data | `std::vector<std::uint8_t>` |
| `CustomBaseClass` | Optional base class for extension | `void` |

### Default Type Aliases

Two default specializations are defined:

```cpp
using json = basic_json<>;
using ordered_json = basic_json<nlohmann::ordered_map>;
```

The `ordered_json` type preserves insertion order by using `ordered_map`
instead of `std::map`.

### Derived Type Aliases

Within `basic_json`, the following public type aliases expose the actual
types used for JSON value storage:

```cpp
using object_t = ObjectType<StringType, basic_json,
                            default_object_comparator_t,
                            AllocatorType<std::pair<const StringType, basic_json>>>;
using array_t = ArrayType<basic_json, AllocatorType<basic_json>>;
using string_t = StringType;
using boolean_t = BooleanType;
using number_integer_t = NumberIntegerType;
using number_unsigned_t = NumberUnsignedType;
using number_float_t = NumberFloatType;
using binary_t = nlohmann::byte_container_with_subtype<BinaryType>;
using object_comparator_t = detail::actual_object_comparator_t<basic_json>;
```

The `default_object_comparator_t` depends on the C++ standard level:
- C++14 and above: `std::less<>` (transparent comparator)
- C++11: `std::less<StringType>`

## Inheritance Structure

### Base Class: `json_base_class`

`basic_json` inherits from `detail::json_base_class<CustomBaseClass>`:

```cpp
class basic_json
    : public ::nlohmann::detail::json_base_class<CustomBaseClass>
```

When `CustomBaseClass` is `void` (the default), this is an empty base class
that adds no overhead. When a user-provided type is specified, it becomes
the base, enabling extension without modifying the library.

### Friend Declarations

The class declares friendship with its internal collaborators:

```cpp
template<detail::value_t> friend struct detail::external_constructor;
template<typename> friend class ::nlohmann::json_pointer;
template<typename BasicJsonType, typename InputType>
    friend class ::nlohmann::detail::parser;
friend ::nlohmann::detail::serializer<basic_json>;
template<typename BasicJsonType>
    friend class ::nlohmann::detail::iter_impl;
template<typename BasicJsonType, typename CharType>
    friend class ::nlohmann::detail::binary_writer;
template<typename BasicJsonType, typename InputType, typename SAX>
    friend class ::nlohmann::detail::binary_reader;
template<typename BasicJsonType, typename InputAdapterType>
    friend class ::nlohmann::detail::json_sax_dom_parser;
template<typename BasicJsonType, typename InputAdapterType>
    friend class ::nlohmann::detail::json_sax_dom_callback_parser;
friend class ::nlohmann::detail::exception;
```

## Memory Layout: `json_value` Union

### The `json_value` Union

The core storage is a union that keeps the `basic_json` object at minimum
size:

```cpp
union json_value
{
    object_t* object;               // pointer — 8 bytes
    array_t* array;                 // pointer — 8 bytes
    string_t* string;              // pointer — 8 bytes
    binary_t* binary;              // pointer — 8 bytes
    boolean_t boolean;             // typically 1 byte
    number_integer_t number_integer; // 8 bytes
    number_unsigned_t number_unsigned; // 8 bytes
    number_float_t number_float;   // 8 bytes

    json_value() = default;
    json_value(boolean_t v) noexcept;
    json_value(number_integer_t v) noexcept;
    json_value(number_unsigned_t v) noexcept;
    json_value(number_float_t v) noexcept;
    json_value(value_t t);         // creates empty container for compound types

    void destroy(value_t t);       // type-aware destructor
};
```

**Key design decisions:**

1. **Pointers for variable-length types.** Objects, arrays, strings, and binaries
   are stored as pointers. This keeps the union at 8 bytes on 64-bit systems
   and avoids calling constructors/destructors for the union members of
   non-active types.

2. **Value semantics for scalars.** Booleans, integers, and floats are stored
   directly in the union without indirection.

3. **Heap allocation via `create<T>()`.** The private static method
   `basic_json::create<T>(Args...)` uses the `AllocatorType` to allocate
   and construct heap objects.

### The `data` Struct

The union is wrapped in a `data` struct that pairs it with the type tag:

```cpp
struct data
{
    value_t m_type = value_t::null;
    json_value m_value = {};

    data(const value_t v);
    data(size_type cnt, const basic_json& val);
    data() noexcept = default;
    data(data&&) noexcept = default;

    ~data() noexcept { m_value.destroy(m_type); }
};
```

The instance lives in `basic_json` as `data m_data`:

```cpp
data m_data = {};      // the type + value

#if JSON_DIAGNOSTICS
basic_json* m_parent = nullptr;  // parent pointer for diagnostics
#endif

#if JSON_DIAGNOSTIC_POSITIONS
std::size_t start_position = std::string::npos;
std::size_t end_position = std::string::npos;
#endif
```

### Destruction Strategy

The `json_value::destroy(value_t)` method handles recursive destruction
without stack overflow. For arrays and objects, it uses an iterative
approach with a `std::vector<basic_json>` stack:

```cpp
void destroy(value_t t) {
    // For arrays/objects: flatten children onto a heap-allocated stack
    if (t == value_t::array || t == value_t::object) {
        std::vector<basic_json> stack;
        // Move children to stack
        while (!stack.empty()) {
            basic_json current_item(std::move(stack.back()));
            stack.pop_back();
            // Move current_item's children to stack
            // current_item safely destructed here (no children)
        }
    }
    // Deallocate the container itself
    switch (t) {
        case value_t::object:  /* deallocate object */ break;
        case value_t::array:   /* deallocate array  */ break;
        case value_t::string:  /* deallocate string */ break;
        case value_t::binary:  /* deallocate binary */ break;
        default: break;
    }
}
```

This prevents stack overflow when destroying deeply nested JSON structures.

## The `value_t` Enumeration

Defined in `detail/value_t.hpp`:

```cpp
enum class value_t : std::uint8_t
{
    null,             // null value
    object,           // unordered set of name/value pairs
    array,            // ordered collection of values
    string,           // string value
    boolean,          // boolean value
    number_integer,   // signed integer
    number_unsigned,  // unsigned integer
    number_float,     // floating-point
    binary,           // binary array
    discarded         // discarded by parser callback
};
```

A comparison operator defines a Python-like ordering:
`null < boolean < number < object < array < string < binary`

With C++20, this uses `std::partial_ordering` via the spaceship operator.

## Class Invariant

The `assert_invariant()` method (called at the end of every constructor)
enforces the following:

```cpp
void assert_invariant(bool check_parents = true) const noexcept
{
    JSON_ASSERT(m_data.m_type != value_t::object || m_data.m_value.object != nullptr);
    JSON_ASSERT(m_data.m_type != value_t::array  || m_data.m_value.array  != nullptr);
    JSON_ASSERT(m_data.m_type != value_t::string || m_data.m_value.string != nullptr);
    JSON_ASSERT(m_data.m_type != value_t::binary || m_data.m_value.binary != nullptr);
}
```

When `JSON_DIAGNOSTICS` is enabled, it additionally checks that all children
have their `m_parent` pointer set to `this`.

## Internal Component Architecture

### Input Pipeline

```
Input Source → Input Adapter → Lexer → Parser → DOM / SAX Events
```

1. **Input Adapters** (`detail/input/input_adapters.hpp`)
   - `file_input_adapter` — wraps `std::FILE*`
   - `input_stream_adapter` — wraps `std::istream`
   - `iterator_input_adapter` — wraps iterator pairs
   - `contiguous_input_adapter` — optimized for contiguous memory

2. **Lexer** (`detail/input/lexer.hpp`)
   - `lexer_base<BasicJsonType>` — defines `token_type` enumeration
   - `lexer<BasicJsonType, InputAdapterType>` — the tokenizer
   - Token types: `literal_true`, `literal_false`, `literal_null`,
     `value_string`, `value_unsigned`, `value_integer`, `value_float`,
     `begin_array`, `begin_object`, `end_array`, `end_object`,
     `name_separator`, `value_separator`, `parse_error`, `end_of_input`

3. **Parser** (`detail/input/parser.hpp`)
   - `parser<BasicJsonType, InputAdapterType>` — recursive descent parser
   - Supports callback-based filtering via `parser_callback_t`
   - Supports both DOM parsing and SAX event dispatch

4. **SAX Interface** (`detail/input/json_sax.hpp`)
   - `json_sax<BasicJsonType>` — abstract base with virtual methods
   - `json_sax_dom_parser` — builds a DOM tree from SAX events
   - `json_sax_dom_callback_parser` — DOM builder with filtering

### Output Pipeline

```
basic_json → Serializer → Output Adapter → Destination
```

1. **Serializer** (`detail/output/serializer.hpp`)
   - `serializer<BasicJsonType>` — converts JSON to text
   - Handles indentation, UTF-8 validation, number formatting
   - `error_handler_t`: `strict`, `replace`, `ignore` for invalid UTF-8

2. **Binary Writer** (`detail/output/binary_writer.hpp`)
   - `binary_writer<BasicJsonType, CharType>` — writes CBOR, MessagePack,
     UBJSON, BJData, BSON

3. **Output Adapters** (`detail/output/output_adapters.hpp`)
   - `output_vector_adapter` — writes to `std::vector<CharType>`
   - `output_stream_adapter` — writes to `std::ostream`
   - `output_string_adapter` — writes to a string type

### Iterator System

```
basic_json::iterator → iter_impl<basic_json>
                     → internal_iterator (union of object/array/primitive iterators)
```

- `iter_impl<BasicJsonType>` — the main iterator class
- `internal_iterator<BasicJsonType>` — holds the active iterator:
  - `typename object_t::iterator object_iterator` for objects
  - `typename array_t::iterator array_iterator` for arrays
  - `primitive_iterator_t` for scalars (0 = begin, 1 = end)
- `json_reverse_iterator<Base>` — reverse iterator adapter
- `iteration_proxy<IteratorType>` — returned by `items()`, exposes
  `key()` and `value()` methods

### Conversion System

The ADL (Argument-Dependent Lookup) design enables seamless integration of
user-defined types:

```
User Type → to_json(json&, const T&) → json value
json value → from_json(const json&, T&) → User Type
```

- `adl_serializer<T>` — default serializer that delegates via ADL
- `detail/conversions/to_json.hpp` — built-in `to_json()` overloads
  for standard types (arithmetic, strings, containers, pairs, tuples)
- `detail/conversions/from_json.hpp` — built-in `from_json()` overloads

### JSON Pointer and Patch

- `json_pointer<RefStringType>` — implements RFC 6901, stores parsed
  reference tokens as `std::vector<string_t>`
- Patch operations implemented directly in `basic_json::patch_inplace()`
  as an inline method operating on the `basic_json` itself

## The `ordered_map` Container

Defined in `include/nlohmann/ordered_map.hpp`:

```cpp
template<class Key, class T, class IgnoredLess = std::less<Key>,
         class Allocator = std::allocator<std::pair<const Key, T>>>
struct ordered_map : std::vector<std::pair<const Key, T>, Allocator>
{
    using key_type = Key;
    using mapped_type = T;
    using Container = std::vector<std::pair<const Key, T>, Allocator>;

    std::pair<iterator, bool> emplace(const key_type& key, T&& t);
    T& operator[](const key_type& key);
    T& at(const key_type& key);
    size_type erase(const key_type& key);
    size_type count(const key_type& key) const;
    iterator find(const key_type& key);
    // ...
};
```

It inherits from `std::vector` and implements map-like operations with
linear search. The `IgnoredLess` parameter exists for API compatibility
with `std::map` but is not used — instead, `std::equal_to<>` (C++14) or
`std::equal_to<Key>` (C++11) is used for key comparison.

## The `byte_container_with_subtype` Class

Wraps binary data with an optional subtype tag for binary formats
(MsgPack ext types, CBOR tags, BSON binary subtypes):

```cpp
template<typename BinaryType>
class byte_container_with_subtype : public BinaryType
{
public:
    using container_type = BinaryType;
    using subtype_type = std::uint64_t;

    void set_subtype(subtype_type subtype_) noexcept;
    constexpr subtype_type subtype() const noexcept;
    constexpr bool has_subtype() const noexcept;
    void clear_subtype() noexcept;

private:
    subtype_type m_subtype = 0;
    bool m_has_subtype = false;
};
```

## Namespace Organization

The library uses inline namespaces for ABI versioning:

```cpp
NLOHMANN_JSON_NAMESPACE_BEGIN  // expands to: namespace nlohmann { inline namespace ... {
// ...
NLOHMANN_JSON_NAMESPACE_END    // expands to: } }
```

The inner inline namespace name encodes configuration flags to prevent
ABI mismatches when different translation units are compiled with
different macro settings. The `detail` sub-namespace is not part of the
public API.

## Template Metaprogramming Techniques

### SFINAE and Type Traits

Located in `detail/meta/type_traits.hpp`, these traits control overload
resolution:

- `is_basic_json<T>` — checks if T is a `basic_json` specialization
- `is_compatible_type<BasicJsonType, T>` — checks if T can be stored
- `is_getable<BasicJsonType, T>` — checks if `get<T>()` works
- `has_from_json<BasicJsonType, T>` — checks for `from_json()` overload
- `has_non_default_from_json<BasicJsonType, T>` — non-void return version
- `is_usable_as_key_type<Comparator, KeyType, T>` — for heterogeneous lookup
- `is_comparable<Comparator, A, B>` — checks comparability

### Priority Tags

The `get_impl()` method uses priority tags (`detail::priority_tag<N>`)
to control overload resolution order:

```cpp
template<typename ValueType>
ValueType get_impl(detail::priority_tag<0>) const;  // standard from_json
template<typename ValueType>
ValueType get_impl(detail::priority_tag<1>) const;  // non-default from_json
template<typename BasicJsonType>
BasicJsonType get_impl(detail::priority_tag<2>) const;  // cross-json conversion
template<typename BasicJsonType>
basic_json get_impl(detail::priority_tag<3>) const;  // identity
template<typename PointerType>
auto get_impl(detail::priority_tag<4>) const;  // pointer access
```

Higher priority tags are preferred during overload resolution.

### External Constructors

The `detail::external_constructor<value_t>` template specializations
handle constructing `json_value` instances for specific types:

```cpp
template<> struct external_constructor<value_t::string>;
template<> struct external_constructor<value_t::number_float>;
template<> struct external_constructor<value_t::number_unsigned>;
template<> struct external_constructor<value_t::number_integer>;
template<> struct external_constructor<value_t::array>;
template<> struct external_constructor<value_t::object>;
template<> struct external_constructor<value_t::boolean>;
template<> struct external_constructor<value_t::binary>;
```

## Diagnostics Architecture

### `JSON_DIAGNOSTICS` Mode

When enabled, each `basic_json` node stores a `m_parent` pointer:

```cpp
#if JSON_DIAGNOSTICS
basic_json* m_parent = nullptr;
#endif
```

The `set_parents()` and `set_parent()` methods maintain these links.
On errors, `exception::diagnostics()` walks the parent chain to build
a JSON Pointer path showing where in the document the error occurred:

```
[json.exception.type_error.302] (/config/debug) type must be boolean, but is string
```

### `JSON_DIAGNOSTIC_POSITIONS` Mode

When enabled, byte offsets from parsing are stored:

```cpp
#if JSON_DIAGNOSTIC_POSITIONS
std::size_t start_position = std::string::npos;
std::size_t end_position = std::string::npos;
#endif
```

Error messages then include `(bytes N-M)` indicating the exact input range.

## Copy and Move Semantics

### Copy Constructor

Deep-copies the value based on type. For compound types (object, array,
string, binary), the heap-allocated data is cloned:

```cpp
basic_json(const basic_json& other)
    : json_base_class_t(other)
{
    m_data.m_type = other.m_data.m_type;
    switch (m_data.m_type) {
        case value_t::object: m_data.m_value = *other.m_data.m_value.object; break;
        case value_t::array:  m_data.m_value = *other.m_data.m_value.array;  break;
        case value_t::string: m_data.m_value = *other.m_data.m_value.string; break;
        // ... scalar types are copied directly
    }
    set_parents();
}
```

### Move Constructor

Transfers ownership and invalidates the source:

```cpp
basic_json(basic_json&& other) noexcept
    : json_base_class_t(std::forward<json_base_class_t>(other)),
      m_data(std::move(other.m_data))
{
    other.m_data.m_type = value_t::null;
    other.m_data.m_value = {};
    set_parents();
}
```

### Copy-and-Swap Assignment

Uses the copy-and-swap idiom for exception safety:

```cpp
basic_json& operator=(basic_json other) noexcept {
    using std::swap;
    swap(m_data.m_type, other.m_data.m_type);
    swap(m_data.m_value, other.m_data.m_value);
    json_base_class_t::operator=(std::move(other));
    set_parents();
    return *this;
}
```

## Comparison Architecture

### C++20 Path (Three-Way Comparison)

When `JSON_HAS_THREE_WAY_COMPARISON` is true:

```cpp
bool operator==(const_reference rhs) const noexcept;
bool operator!=(const_reference rhs) const noexcept;
std::partial_ordering operator<=>(const_reference rhs) const noexcept;
```

### Pre-C++20 Path

Individual comparison operators are defined as `friend` functions:

```cpp
friend bool operator==(const_reference lhs, const_reference rhs) noexcept;
friend bool operator!=(const_reference lhs, const_reference rhs) noexcept;
friend bool operator<(const_reference lhs, const_reference rhs) noexcept;
friend bool operator<=(const_reference lhs, const_reference rhs) noexcept;
friend bool operator>(const_reference lhs, const_reference rhs) noexcept;
friend bool operator>=(const_reference lhs, const_reference rhs) noexcept;
```

Both paths use the `JSON_IMPLEMENT_OPERATOR` macro internally, which handles:
1. Same-type comparison (delegates to underlying type's operator)
2. Cross-numeric-type comparison (int vs float, signed vs unsigned)
3. Unordered comparison (NaN, discarded values)
4. Different-type comparison (compares `value_t` ordering)

## `std` Namespace Specializations

The library provides:

```cpp
namespace std {
    template<> struct hash<nlohmann::json> { ... };
    template<> struct less<nlohmann::detail::value_t> { ... };
    void swap(nlohmann::json& j1, nlohmann::json& j2) noexcept;  // pre-C++20 only
}
```

The hash function delegates to `nlohmann::detail::hash()` which recursively
hashes the JSON value based on its type.
