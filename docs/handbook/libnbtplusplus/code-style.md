# Code Style & Conventions

## Overview

This document describes the coding conventions and patterns observed throughout the libnbt++ codebase. These are not arbitrary style choices — they reflect deliberate design decisions for a C++11 library focused on correctness, interoperability, and clean ownership semantics.

---

## Namespaces

### Primary Namespaces

| Namespace | Purpose | Location |
|-----------|---------|----------|
| `nbt` | All public API types: tags, value, visitors | `include/` |
| `nbt::detail` | Internal implementation details | `include/crtp_tag.h`, `include/tag_primitive.h` |
| `nbt::io` | Binary serialization (stream_reader, stream_writer) | `include/io/` |
| `nbt::text` | Text formatting (json_formatter) | `include/text/` |
| `endian` | Byte-order conversion functions | `include/endian_str.h` |
| `zlib` | Compression stream wrappers | `include/io/izlibstream.h`, `include/io/ozlibstream.h` |

### Namespace Usage

- All user-facing code is in the `nbt` namespace
- Internal helpers like `crtp_tag` and `make_unique` are in `nbt::detail` (not `nbt`)
- The `endian` and `zlib` namespaces are top-level, **not** nested under `nbt`
- No `using namespace` directives appear in headers

---

## Export Macro

```cpp
#ifdef NBT_SHARED
    #ifdef NBT_BUILD
        #define NBT_EXPORT __declspec(dllexport)  // When building the shared lib
    #else
        #define NBT_EXPORT __declspec(dllimport)  // When consuming the shared lib
    #endif
#else
    #define NBT_EXPORT  // Static build: empty
#endif
```

`NBT_EXPORT` is applied to all public classes and free functions:

```cpp
class NBT_EXPORT tag { ... };
class NBT_EXPORT tag_list final : public detail::crtp_tag<tag_list> { ... };
NBT_EXPORT bool operator==(const tag_compound& lhs, const tag_compound& rhs);
```

Classes in `nbt::detail` (like `crtp_tag`) are **not** exported.

---

## Class Design Patterns

### final Classes

All concrete tag classes are `final`:

```cpp
class tag_list final : public detail::crtp_tag<tag_list> { ... };
class tag_compound final : public detail::crtp_tag<tag_compound> { ... };
class tag_string final : public detail::crtp_tag<tag_string> { ... };
```

This prevents further inheritance and enables compiler devirtualization optimizations.

### CRTP Intermediate

The Curiously Recurring Template Pattern eliminates boilerplate:

```cpp
template <class Sub>
class crtp_tag : public tag
{
    tag_type get_type() const noexcept override { return Sub::type; }
    std::unique_ptr<tag> clone() const& override { return make_unique<Sub>(/*copy*/); }
    std::unique_ptr<tag> move_clone() && override { return make_unique<Sub>(std::move(/*this*/)); }
    void accept(nbt_visitor& visitor) const override { visitor.visit(const_cast<Sub&>(...)); }
    // ...
};
```

Each concrete class inherits from `crtp_tag<Self>` and gets all 6 virtual method implementations for free.

### Static Type Constants

Each tag class exposes its type as a `static constexpr`:

```cpp
class tag_int final : public detail::crtp_tag<tag_int>
{
public:
    static constexpr tag_type type = tag_type::Int;
    // ...
};
```

Used for compile-time type checks and template metaprogramming.

---

## Ownership Conventions

### Unique Pointer Everywhere

All tag ownership uses `std::unique_ptr<tag>`:

```cpp
std::unique_ptr<tag> tag::create(tag_type type);
std::unique_ptr<tag> tag::clone() const&;
std::unique_ptr<tag> stream_reader::read_payload(tag_type type);
```

### Custom make_unique

Since `std::make_unique` is C++14, the library provides its own in `nbt::detail`:

```cpp
namespace detail {
    template <class T, class... Args>
    std::unique_ptr<T> make_unique(Args&&... args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}
```

Used throughout source files instead of raw `new`:

```cpp
tags.emplace_back(make_unique<T>(std::forward<Args>(args)...));
```

### value as Type-Erased Wrapper

The `value` class wraps `std::unique_ptr<tag>` to provide implicit conversions and operator overloading that `unique_ptr` cannot support:

```cpp
class value
{
    std::unique_ptr<tag> tag_;
public:
    value& operator=(int32_t val);    // Assigns to contained tag
    operator int32_t() const;         // Reads from contained tag
    value& operator[](const std::string& key); // Delegates to tag_compound
    // ...
};
```

---

## C++11 Features Used

| Feature | Usage |
|---------|-------|
| `std::unique_ptr` | All tag ownership |
| Move semantics | `value(value&&)`, `tag_list::push_back(value_initializer&&)` |
| `override` | All virtual method overrides |
| `final` | All concrete tag classes |
| `constexpr` | Static type constants, writer limits |
| `noexcept` | `get_type()`, visitor destructors |
| `std::initializer_list` | Compound and list construction |
| Range-based for | Internal iteration in compounds, lists |
| `auto` | Type deduction in local variables |
| `static_assert` | Endian implementation checks |
| `enum class` | `tag_type`, `endian::endian` |
| Variadic templates | `emplace_back<T>(Args&&...)`, `make_unique` |

The library does **not** use C++14 or later features, maintaining broad compiler compatibility.

---

## Include Structure

### Public Headers (for Library Users)

```
include/
  tag.h              // tag base, tag_type enum, create(), as<T>()
  crtp_tag.h         // CRTP intermediate (detail)
  nbt_tags.h         // Master include — includes all tag headers
  tag_primitive.h    // tag_byte, tag_short, tag_int, tag_long, tag_float, tag_double
  tag_string.h       // tag_string
  tag_array.h        // tag_byte_array, tag_int_array, tag_long_array
  tag_list.h         // tag_list
  tag_compound.h     // tag_compound
  value.h            // value wrapper
  value_initializer.h // value_initializer for implicit construction
  nbt_visitor.h      // nbt_visitor, const_nbt_visitor
  endian_str.h       // endian read/write functions
  io/
    stream_reader.h  // stream_reader, read_compound(), read_tag()
    stream_writer.h  // stream_writer, write_tag()
    izlibstream.h    // izlibstream (decompression)
    ozlibstream.h    // ozlibstream (compression)
    zlib_streambuf.h // Base streambuf for zlib
  text/
    json_formatter.h // json_formatter
```

### Master Include

`nbt_tags.h` includes all tag types for convenience:

```cpp
// nbt_tags.h
#include "tag.h"
#include "tag_primitive.h"
#include "tag_string.h"
#include "tag_array.h"
#include "tag_list.h"
#include "tag_compound.h"
#include "value.h"
#include "value_initializer.h"
```

Users can include individual headers for faster compilation or `nbt_tags.h` for convenience.

---

## Error Handling Style

### Exceptions for Programmer Errors

- `std::invalid_argument`: Type mismatches in lists, null values
- `std::out_of_range`: Invalid indices in lists, missing keys in compounds
- `std::logic_error`: Inconsistent internal state

### Exceptions for I/O Errors

- `io::input_error` (extends `std::runtime_error`): All parse/read failures
- `std::runtime_error`: Write stream failures
- `std::length_error`: Exceeding NBT format limits
- `zlib::zlib_error` (extends `std::runtime_error`): Compression/decompression failures

### Stream State

Reader/writer methods check `is` / `os` state after I/O operations and throw on failure. Write methods set failbit before throwing to maintain consistent stream state.

---

## Naming Conventions

| Element | Convention | Examples |
|---------|-----------|----------|
| Classes | `snake_case` | `tag_compound`, `stream_reader`, `tag_list` |
| Methods | `snake_case` | `get_type()`, `read_payload()`, `el_type()` |
| Member variables | `snake_case` with trailing `_` | `el_type_`, `tag_`, `is`, `os` |
| Template parameters | `PascalCase` | `Sub`, `T`, `Args` |
| Enum values | `PascalCase` | `tag_type::Byte_Array`, `tag_type::Long_Array` |
| Namespaces | `snake_case` | `nbt`, `nbt::io`, `nbt::detail` |
| Macros | `UPPER_SNAKE_CASE` | `NBT_EXPORT`, `NBT_HAVE_ZLIB`, `NBT_SHARED` |
| Constants | `snake_case` | `max_string_len`, `max_array_len`, `MAX_DEPTH` |

---

## Template Patterns

### Explicit Instantiation

Template classes like `tag_primitive<T>` and `tag_array<T>` use extern template declarations in headers and explicit instantiation in source files:

```cpp
// In tag_primitive.h
extern template class tag_primitive<int8_t>;
extern template class tag_primitive<int16_t>;
extern template class tag_primitive<int32_t>;
extern template class tag_primitive<int64_t>;
extern template class tag_primitive<float>;
extern template class tag_primitive<double>;

// In tag.cpp
template class tag_primitive<int8_t>;
template class tag_primitive<int16_t>;
// ...
```

This prevents duplicate template instantiation across translation units, reducing compile time and binary size.

### Type Aliases

```cpp
typedef tag_primitive<int8_t>  tag_byte;
typedef tag_primitive<int16_t> tag_short;
typedef tag_primitive<int32_t> tag_int;
typedef tag_primitive<int64_t> tag_long;
typedef tag_primitive<float>   tag_float;
typedef tag_primitive<double>  tag_double;

typedef tag_array<int8_t>  tag_byte_array;
typedef tag_array<int32_t> tag_int_array;
typedef tag_array<int64_t> tag_long_array;
```

Uses `typedef` rather than `using` — a C++11 compatibility choice, though both are equivalent.
