# libnbt++ Overview

## What is libnbt++?

libnbt++ is a free C++ library for reading, writing, and manipulating Minecraft's **Named Binary Tag (NBT)** file format. It provides a modern C++11 interface for working with NBT data, supporting both compressed and uncompressed files, big-endian (Java Edition) and little-endian (Bedrock/Pocket Edition) byte orders, and full tag hierarchy manipulation.

The library lives under the `nbt` namespace and provides strongly-typed tag classes that mirror the NBT specification exactly. It was originally created by ljfa-ag and is licensed under the GNU Lesser General Public License v3.0 (LGPL-3.0-or-later).

libnbt++3 is a complete rewrite of the older libnbt++2, designed to eliminate boilerplate code and provide a more natural C++ syntax for NBT operations.

---

## The NBT Format

NBT (Named Binary Tag) is a binary serialization format invented by Markus "Notch" Persson for Minecraft. It is used throughout the game to store:

- World save data (level.dat)
- Chunk data (region files)
- Player inventories and statistics
- Structure files
- Server configuration

### Binary Structure

An NBT file consists of a single named root tag, which is always a **Compound** tag. The binary layout is:

```
[tag type byte] [name length (2 bytes, big-endian)] [name (UTF-8)] [payload]
```

Each tag type has a specific format for its payload, and compound/list tags recursively contain other tags.

### Compression

NBT files in Minecraft are typically compressed with either **gzip** (most common for `.dat` files) or **zlib/deflate** (used in chunk data within region files). libnbt++ supports both through its optional zlib integration.

---

## Tag Types

The NBT format defines 13 tag types, represented in libnbt++ by the `tag_type` enum class defined in `include/tag.h`:

```cpp
enum class tag_type : int8_t {
    End        = 0,   // Marks the end of a compound tag
    Byte       = 1,   // Signed 8-bit integer
    Short      = 2,   // Signed 16-bit integer
    Int        = 3,   // Signed 32-bit integer
    Long       = 4,   // Signed 64-bit integer
    Float      = 5,   // 32-bit IEEE 754 floating point
    Double     = 6,   // 64-bit IEEE 754 floating point
    Byte_Array = 7,   // Array of signed bytes
    String     = 8,   // UTF-8 string (max 65535 bytes)
    List       = 9,   // Ordered list of unnamed tags (same type)
    Compound   = 10,  // Collection of named tags (any type)
    Int_Array  = 11,  // Array of signed 32-bit integers
    Long_Array = 12,  // Array of signed 64-bit integers
    Null       = -1   // Internal: denotes empty value objects
};
```

The `Null` type (value -1) is an internal sentinel used by libnbt++ to represent uninitialized `value` objects; it does not appear in the NBT specification.

The `End` type (value 0) is only valid within compound tags to mark their end; it is never used as a standalone tag.

### Tag Type Validation

The function `is_valid_type()` checks whether an integer value is a valid tag type:

```cpp
bool is_valid_type(int type, bool allow_end = false);
```

It returns `true` when `type` falls between 1 and 12 (inclusive), or between 0 and 12 if `allow_end` is `true`.

---

## C++ Tag Classes

Each NBT tag type maps to a concrete C++ class in the `nbt` namespace. The classes are organized using templates for related types:

| NBT Type     | ID | C++ Class          | Underlying Type         | Header             |
|-------------|----|--------------------|------------------------|--------------------|
| Byte        |  1 | `tag_byte`         | `tag_primitive<int8_t>` | `tag_primitive.h`  |
| Short       |  2 | `tag_short`        | `tag_primitive<int16_t>`| `tag_primitive.h`  |
| Int         |  3 | `tag_int`          | `tag_primitive<int32_t>`| `tag_primitive.h`  |
| Long        |  4 | `tag_long`         | `tag_primitive<int64_t>`| `tag_primitive.h`  |
| Float       |  5 | `tag_float`        | `tag_primitive<float>`  | `tag_primitive.h`  |
| Double      |  6 | `tag_double`       | `tag_primitive<double>` | `tag_primitive.h`  |
| Byte_Array  |  7 | `tag_byte_array`   | `tag_array<int8_t>`     | `tag_array.h`      |
| String      |  8 | `tag_string`       | `tag_string`            | `tag_string.h`     |
| List        |  9 | `tag_list`         | `tag_list`              | `tag_list.h`       |
| Compound    | 10 | `tag_compound`     | `tag_compound`          | `tag_compound.h`   |
| Int_Array   | 11 | `tag_int_array`    | `tag_array<int32_t>`    | `tag_array.h`      |
| Long_Array  | 12 | `tag_long_array`   | `tag_array<int64_t>`    | `tag_array.h`      |

The typedef names (`tag_byte`, `tag_short`, etc.) are the intended public API. The underlying template classes (`tag_primitive<T>`, `tag_array<T>`) should not be used directly.

---

## Library Features

### Modern C++11 Design

- **Move semantics**: Tags support move construction and move assignment for efficient transfers
- **Smart pointers**: `std::unique_ptr<tag>` is used throughout for ownership management
- **Initializer lists**: Compounds and lists can be constructed with brace-enclosed initializer lists
- **Type-safe conversions**: The `value` class provides explicit conversions with `std::bad_cast` on type mismatch
- **Templates**: `tag_primitive<T>` and `tag_array<T>` use templates to avoid code duplication

### Convenient Syntax

Creating complex NBT structures is straightforward:

```cpp
#include <nbt_tags.h>

nbt::tag_compound root{
    {"playerName", "Steve"},
    {"health", int16_t(20)},
    {"position", nbt::tag_list{1.0, 64.5, -3.2}},
    {"inventory", nbt::tag_list::of<nbt::tag_compound>({
        {{"id", "minecraft:diamond_sword"}, {"count", int8_t(1)}},
        {{"id", "minecraft:apple"},         {"count", int8_t(64)}}
    })},
    {"scores", nbt::tag_int_array{100, 250, 380}}
};
```

### The value Class

The `value` class (`include/value.h`) acts as a type-erased wrapper around `std::unique_ptr<tag>`. It enables:

- Implicit numeric conversions (widening only): `int8_t` → `int16_t` → `int32_t` → `int64_t` → `float` → `double`
- Direct string assignment
- Subscript access: `compound["key"]` for compounds, `list[index]` for lists
- Chained access: `root["nested"]["deep"]["value"]`

### I/O System

Reading and writing NBT data uses the stream-based API:

```cpp
#include <nbt_tags.h>
#include <io/stream_reader.h>
#include <io/stream_writer.h>
#include <fstream>

// Reading
std::ifstream file("level.dat", std::ios::binary);
auto [name, compound] = nbt::io::read_compound(file);

// Writing
std::ofstream out("output.nbt", std::ios::binary);
nbt::io::write_tag("Level", *compound, out);
```

The I/O system supports both big-endian (Java Edition default) and little-endian (Bedrock Edition) byte orders via the `endian::endian` enum:

```cpp
// Reading Bedrock Edition data
auto pair = nbt::io::read_compound(file, endian::little);
```

### Zlib Compression Support

When built with `NBT_USE_ZLIB=ON` (the default), the library provides stream wrappers for transparent compression/decompression:

```cpp
#include <io/izlibstream.h>
#include <io/ozlibstream.h>

// Reading gzip-compressed NBT
std::ifstream gzfile("level.dat", std::ios::binary);
zlib::izlibstream decompressed(gzfile);
auto pair = nbt::io::read_compound(decompressed);

// Writing gzip-compressed NBT
std::ofstream outfile("output.dat", std::ios::binary);
zlib::ozlibstream compressed(outfile, Z_DEFAULT_COMPRESSION, true /* gzip */);
nbt::io::write_tag("Level", root, compressed);
compressed.close();
```

### Visitor Pattern

The library implements the Visitor pattern through `nbt_visitor` and `const_nbt_visitor` base classes, with 12 overloads (one per concrete tag type). The JSON formatter (`text::json_formatter`) is an example of a visitor that pretty-prints tag trees for debugging.

### Polymorphic Operations

All tag classes support:

- **`clone()`** — Deep-copies the tag, returning `std::unique_ptr<tag>`
- **`move_clone()`** — Moves the tag into a new `unique_ptr`
- **`assign(tag&&)`** — Move-assigns from another tag of the same type
- **`get_type()`** — Returns the `tag_type` enum value
- **`operator==` / `operator!=`** — Deep equality comparison
- **`operator<<`** — JSON-like formatted output via `text::json_formatter`

### Factory Construction

Tags can be constructed dynamically by type:

```cpp
auto t = nbt::tag::create(nbt::tag_type::Int);         // Default-constructed tag_int(0)
auto t = nbt::tag::create(nbt::tag_type::Float, 3.14f); // Numeric tag_float(3.14)
```

---

## Namespace Organization

| Namespace       | Contents                                                    |
|----------------|-------------------------------------------------------------|
| `nbt`          | All tag classes, `value`, `value_initializer`, helpers       |
| `nbt::detail`  | CRTP base class, primitive/array type traits (internal)      |
| `nbt::io`      | `stream_reader`, `stream_writer`, free functions             |
| `nbt::text`    | `json_formatter` for pretty-printing                        |
| `endian`       | Endianness-aware binary read/write functions                 |
| `zlib`         | zlib stream wrappers (`izlibstream`, `ozlibstream`)          |

---

## File Organization

### Public Headers (`include/`)

| File                      | Purpose                                                     |
|--------------------------|-------------------------------------------------------------|
| `tag.h`                  | `tag` base class, `tag_type` enum, `is_valid_type()`         |
| `tagfwd.h`               | Forward declarations for all tag classes                     |
| `nbt_tags.h`             | Convenience header — includes all tag headers                |
| `tag_primitive.h`        | `tag_primitive<T>` template and `tag_byte`..`tag_double` typedefs |
| `tag_string.h`           | `tag_string` class                                           |
| `tag_array.h`            | `tag_array<T>` template and `tag_byte_array`..`tag_long_array` |
| `tag_list.h`             | `tag_list` class                                             |
| `tag_compound.h`         | `tag_compound` class                                         |
| `value.h`                | `value` type-erased tag wrapper                              |
| `value_initializer.h`    | `value_initializer` — implicit conversions for function params |
| `crtp_tag.h`             | CRTP base template implementing polymorphic dispatch          |
| `primitive_detail.h`     | Type traits mapping C++ types to `tag_type` values           |
| `nbt_visitor.h`          | `nbt_visitor` and `const_nbt_visitor` base classes           |
| `endian_str.h`           | Endianness-aware binary I/O functions                        |
| `make_unique.h`          | `nbt::make_unique<T>()` helper (C++11 polyfill)             |
| `io/stream_reader.h`     | `stream_reader` class and `read_compound()`/`read_tag()`     |
| `io/stream_writer.h`     | `stream_writer` class and `write_tag()`                      |
| `io/izlibstream.h`       | `izlibstream` for decompression (requires zlib)              |
| `io/ozlibstream.h`       | `ozlibstream` for compression (requires zlib)                |
| `io/zlib_streambuf.h`    | `zlib_streambuf` base class, `zlib_error` exception          |
| `text/json_formatter.h`  | `json_formatter` for pretty-printing tags                    |

### Source Files (`src/`)

| File                       | Purpose                                  |
|---------------------------|------------------------------------------|
| `tag.cpp`                 | `tag` methods, `tag_primitive` explicit instantiations, operators |
| `tag_compound.cpp`        | `tag_compound` methods, binary I/O       |
| `tag_list.cpp`            | `tag_list` methods, initializer lists, binary I/O |
| `tag_string.cpp`          | `tag_string` read/write payload          |
| `value.cpp`               | `value` assignment operators, conversions |
| `value_initializer.cpp`   | `value_initializer` constructors         |
| `endian_str.cpp`          | Big/little endian read/write implementations |
| `io/stream_reader.cpp`    | `stream_reader` methods, format parsing  |
| `io/stream_writer.cpp`    | `stream_writer` methods, format output   |
| `io/izlibstream.cpp`      | `inflate_streambuf` implementation       |
| `io/ozlibstream.cpp`      | `deflate_streambuf` implementation       |
| `text/json_formatter.cpp` | `json_formatter` visitor implementation  |

---

## Quick Start Examples

### Reading an NBT File

```cpp
#include <nbt_tags.h>
#include <io/stream_reader.h>
#include <fstream>
#include <iostream>

int main() {
    std::ifstream file("level.dat", std::ios::binary);
    if (!file) return 1;

    auto [name, root] = nbt::io::read_compound(file);
    std::cout << "Root tag: " << name << "\n";
    std::cout << *root << std::endl;  // JSON-formatted output

    return 0;
}
```

### Reading a Compressed File

```cpp
#include <nbt_tags.h>
#include <io/stream_reader.h>
#include <io/izlibstream.h>
#include <fstream>

int main() {
    std::ifstream file("level.dat", std::ios::binary);
    zlib::izlibstream decompressed(file);  // Auto-detects gzip/zlib
    auto [name, root] = nbt::io::read_compound(decompressed);
    return 0;
}
```

### Creating and Writing NBT Data

```cpp
#include <nbt_tags.h>
#include <io/stream_writer.h>
#include <fstream>

int main() {
    nbt::tag_compound data{
        {"name", "World1"},
        {"seed", int64_t(123456789)},
        {"spawnX", int32_t(0)},
        {"spawnY", int32_t(64)},
        {"spawnZ", int32_t(0)},
        {"gameType", int32_t(0)},
        {"raining", int8_t(0)},
        {"version", nbt::tag_compound{
            {"id", int32_t(19133)},
            {"name", "1.20.4"},
            {"snapshot", int8_t(0)}
        }}
    };

    std::ofstream out("output.nbt", std::ios::binary);
    nbt::io::write_tag("", data, out);
    return 0;
}
```

### Modifying Existing Data

```cpp
auto [name, root] = nbt::io::read_compound(file);

// Modify values using operator[]
(*root)["playerName"] = std::string("Alex");
(*root)["health"] = int16_t(20);

// Add a new nested compound
root->put("newSection", nbt::tag_compound{
    {"key1", int32_t(42)},
    {"key2", "hello"}
});

// Remove a tag
root->erase("oldSection");

// Check if a key exists
if (root->has_key("inventory", nbt::tag_type::List)) {
    auto& inv = root->at("inventory").as<nbt::tag_list>();
    inv.push_back(nbt::tag_compound{{"id", "minecraft:stone"}, {"count", int8_t(1)}});
}
```

### Iterating Over Tags

```cpp
// Iterating a compound
for (const auto& [key, val] : *root) {
    std::cout << key << ": type=" << val.get_type() << "\n";
}

// Iterating a list
auto& list = root->at("items").as<nbt::tag_list>();
for (size_t i = 0; i < list.size(); ++i) {
    std::cout << "Item " << i << ": " << list[i].get() << "\n";
}
```

---

## Error Handling

libnbt++ uses exceptions for error reporting:

| Exception               | Thrown When                                              |
|------------------------|----------------------------------------------------------|
| `nbt::io::input_error` | Read failure: invalid tag type, unexpected EOF, corruption |
| `std::bad_cast`        | Type mismatch in `value` conversions or `tag::assign()`   |
| `std::out_of_range`    | Invalid key in `tag_compound::at()` or index in `tag_list::at()` |
| `std::invalid_argument`| Invalid tag type to `tag::create()`, type mismatch in list operations |
| `std::length_error`    | String > 65535 bytes, array > INT32_MAX elements          |
| `zlib::zlib_error`     | zlib decompression/compression failure                    |
| `std::bad_alloc`       | zlib memory allocation failure                            |

Stream state flags (`failbit`, `badbit`) are also set on the underlying `std::istream`/`std::ostream` when errors occur.

---

## Thread Safety

libnbt++ provides no thread safety guarantees beyond those of the C++ standard library. Tag objects should not be accessed concurrently from multiple threads without external synchronization. Reading from separate `stream_reader` instances using independent streams is safe.

---

## Platform Requirements

- C++11 compatible compiler (GCC 4.8+, Clang 3.3+, MSVC 2015+)
- CMake 3.15 or later
- zlib (optional, for compressed NBT support)
- IEEE 754 floating point (enforced via `static_assert`)
- 8-bit bytes (enforced via `static_assert` on `CHAR_BIT`)

The library uses `memcpy`-based type punning (not `reinterpret_cast`) for float/double endian conversions, ensuring defined behavior across compilers.

---

## License

libnbt++ is licensed under the **GNU Lesser General Public License v3.0 or later** (LGPL-3.0-or-later). This means:

- You can link against libnbt++ from proprietary software
- Modifications to libnbt++ itself must be released under LGPL
- The full license text is in `COPYING` and `COPYING.LESSER`
