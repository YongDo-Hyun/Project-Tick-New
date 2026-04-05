# I/O System

## Overview

The `nbt::io` namespace provides the binary serialization layer for reading and writing NBT data. The two central classes are `stream_reader` and `stream_writer`, both operating on standard C++ streams (`std::istream` / `std::ostream`).

Defined in:
- `include/io/stream_reader.h` / `src/io/stream_reader.cpp`
- `include/io/stream_writer.h` / `src/io/stream_writer.cpp`

---

## stream_reader

### Class Definition

```cpp
class NBT_EXPORT stream_reader
{
public:
    explicit stream_reader(std::istream& is,
                           endian::endian e = endian::endian::big);

    std::istream& get_istr() const { return is; }
    endian::endian get_endian() const { return endian; }

    // Read named + typed tags
    std::pair<std::string, std::unique_ptr<tag>> read_tag();

    // Read payload only (for tags whose type is already known)
    std::unique_ptr<tag> read_payload(tag_type type);

    // Read a type byte
    tag_type read_type(bool allow_end);

    // Read a length-prefixed UTF-8 string
    std::string read_string();

    // Read a numeric value in the configured endianness
    template <class T> void read_num(T& x);

    static const unsigned int MAX_DEPTH = 1024;

private:
    std::istream& is;
    endian::endian endian;
    unsigned int depth_ = 0;
};
```

### Constructor

```cpp
stream_reader(std::istream& is, endian::endian e = endian::endian::big);
```

- `is`: The input stream to read from
- `e`: Byte order — `endian::big` (default, Java edition NBT) or `endian::little` (Bedrock edition)

### read_tag() — Read a Complete Named Tag

```cpp
std::pair<std::string, std::unique_ptr<tag>> read_tag();
```

Reads a complete tag from the stream:
1. Reads the type byte
2. If type is `End`, returns `{"", nullptr}` (end-of-compound sentinel)
3. Reads the name string
4. Reads the payload via `read_payload()`

Returns a pair of `{name, tag_ptr}`.

Implementation:
```cpp
std::pair<std::string, std::unique_ptr<tag>>
stream_reader::read_tag()
{
    tag_type type = read_type(true);
    if (type == tag_type::End)
        return {"", nullptr};

    std::string name = read_string();
    auto tag = read_payload(type);
    return {std::move(name), std::move(tag)};
}
```

### read_payload() — Read a Tag Payload

```cpp
std::unique_ptr<tag> read_payload(tag_type type);
```

Creates a tag of the specified type, then calls its `read_payload()` virtual method. Tracks recursive nesting depth, throwing `io::input_error` if `MAX_DEPTH` (1024) is exceeded.

Implementation:
```cpp
std::unique_ptr<tag> stream_reader::read_payload(tag_type type)
{
    if (++depth_ > MAX_DEPTH)
        throw input_error("Maximum nesting depth exceeded");

    auto ret = tag::create(type);
    ret->read_payload(*this);

    --depth_;
    return ret;
}
```

The `tag::create()` factory instantiates the correct concrete class:
```cpp
std::unique_ptr<tag> tag::create(tag_type type)
{
    switch (type) {
        case tag_type::Byte:       return make_unique<tag_byte>();
        case tag_type::Short:      return make_unique<tag_short>();
        case tag_type::Int:        return make_unique<tag_int>();
        case tag_type::Long:       return make_unique<tag_long>();
        case tag_type::Float:      return make_unique<tag_float>();
        case tag_type::Double:     return make_unique<tag_double>();
        case tag_type::Byte_Array: return make_unique<tag_byte_array>();
        case tag_type::String:     return make_unique<tag_string>();
        case tag_type::List:       return make_unique<tag_list>();
        case tag_type::Compound:   return make_unique<tag_compound>();
        case tag_type::Int_Array:  return make_unique<tag_int_array>();
        case tag_type::Long_Array: return make_unique<tag_long_array>();
        default:
            throw std::invalid_argument("Invalid tag type: "
                + std::to_string(static_cast<int>(type)));
    }
}
```

### read_type() — Read and Validate Type Byte

```cpp
tag_type read_type(bool allow_end);
```

Reads a single byte, casts to `tag_type`, and validates:
```cpp
tag_type stream_reader::read_type(bool allow_end)
{
    int type = is.get();
    if (!is)
        throw input_error("Error reading tag type");
    if (!is_valid_type(type, allow_end))
        throw input_error("Invalid tag type: "
            + std::to_string(type));
    return static_cast<tag_type>(type);
}
```

The `allow_end` parameter controls whether `tag_type::End` (0) is accepted — it's valid when reading list element types or compound children, but not at the top level of a standalone tag.

### read_string() — Read Length-Prefixed String

```cpp
std::string read_string();
```

Reads a 2-byte unsigned length, then that many bytes of UTF-8 data:
```cpp
std::string stream_reader::read_string()
{
    uint16_t len;
    read_num(len);
    if (!is)
        throw input_error("Error reading string length");
    std::string str(len, '\0');
    is.read(&str[0], len);
    if (!is)
        throw input_error("Error reading string");
    return str;
}
```

Maximum string length: 65535 bytes (uint16_t max).

### read_num() — Read Numeric Value

```cpp
template <class T> void read_num(T& x)
{
    endian::read(is, x, endian);
}
```

Delegates to the `endian` namespace for endianness-appropriate reading.

---

## stream_writer

### Class Definition

```cpp
class NBT_EXPORT stream_writer
{
public:
    explicit stream_writer(std::ostream& os,
                           endian::endian e = endian::endian::big);

    std::ostream& get_ostr() const { return os; }
    endian::endian get_endian() const { return endian; }

    void write_type(tag_type type);
    void write_string(const std::string& str);
    void write_payload(const tag& t);
    template <class T> void write_num(T x);

    static constexpr size_t max_string_len = UINT16_MAX;
    static constexpr int32_t max_array_len = INT32_MAX;

private:
    std::ostream& os;
    endian::endian endian;
};
```

### Constructor

```cpp
stream_writer(std::ostream& os, endian::endian e = endian::endian::big);
```

- `os`: The output stream to write to
- `e`: Byte order — `endian::big` (default) or `endian::little`

### write_tag() — Free Function

```cpp
void write_tag(const std::string& name, const tag& t,
               std::ostream& os,
               endian::endian e = endian::endian::big);
```

This is a **free function** (not a member). It writes a complete named tag:
1. Writes the type byte
2. Writes the name string
3. Writes the payload

```cpp
void write_tag(const std::string& name, const tag& t,
               std::ostream& os, endian::endian e)
{
    stream_writer writer(os, e);
    writer.write_type(t.get_type());
    writer.write_string(name);
    t.write_payload(writer);
}
```

### write_type() — Write Type Byte

```cpp
void stream_writer::write_type(tag_type type)
{
    os.put(static_cast<char>(type));
    if (!os)
        throw std::runtime_error("Error writing tag type");
}
```

### write_string() — Write Length-Prefixed String

```cpp
void stream_writer::write_string(const std::string& str)
{
    if (str.size() > max_string_len) {
        os.setstate(std::ios::failbit);
        throw std::length_error("String is too long for NBT");
    }
    write_num(static_cast<uint16_t>(str.size()));
    os.write(str.data(), str.size());
    if (!os)
        throw std::runtime_error("Error writing string");
}
```

Strings longer than 65535 bytes trigger a `std::length_error`.

### write_payload() — Write Tag Payload

```cpp
void stream_writer::write_payload(const tag& t)
{
    t.write_payload(*this);
}
```

Delegates to the tag's virtual `write_payload()` method.

### write_num() — Write Numeric Value

```cpp
template <class T> void write_num(T x)
{
    endian::write(os, x, endian);
}
```

---

## Free Functions

### Reading

```cpp
// In nbt::io namespace

std::pair<std::string, std::unique_ptr<tag>>
read_compound(std::istream& is,
              endian::endian e = endian::endian::big);

std::pair<std::string, std::unique_ptr<tag>>
read_tag(std::istream& is,
         endian::endian e = endian::endian::big);
```

**`read_compound()`** reads and validates that the top-level tag is a compound:

```cpp
std::pair<std::string, std::unique_ptr<tag>>
read_compound(std::istream& is, endian::endian e)
{
    stream_reader reader(is, e);
    auto result = reader.read_tag();
    if (!result.second || result.second->get_type() != tag_type::Compound)
        throw input_error("Top-level tag is not a compound");
    return result;
}
```

**`read_tag()`** reads any tag without type restriction:

```cpp
std::pair<std::string, std::unique_ptr<tag>>
read_tag(std::istream& is, endian::endian e)
{
    stream_reader reader(is, e);
    return reader.read_tag();
}
```

### Writing

```cpp
void write_tag(const std::string& name, const tag& t,
               std::ostream& os,
               endian::endian e = endian::endian::big);
```

Writes a complete named tag (type + name + payload). See above.

---

## Error Handling

### input_error

```cpp
class input_error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};
```

Thrown by `stream_reader` for all parse errors:
- Invalid tag type bytes
- Stream read failures
- Negative array/list lengths
- Maximum nesting depth exceeded
- Corrupt or truncated data

### Stream State Errors

Write errors set stream failbit and throw:
- `std::runtime_error` for general write failures
- `std::length_error` for strings exceeding `max_string_len` (65535 bytes)
- `std::length_error` for arrays/lists exceeding `max_array_len` (INT32_MAX elements)
- `std::logic_error` for list type inconsistencies during write

---

## Payload Format Per Tag Type

Each concrete tag class implements its own `read_payload()` and `write_payload()`:

### Primitives (tag_byte, tag_short, tag_int, tag_long, tag_float, tag_double)

```cpp
// In tag_primitive.h (inline)
void read_payload(io::stream_reader& reader) override
{
    reader.read_num(val);
}

void write_payload(io::stream_writer& writer) const override
{
    writer.write_num(val);
}
```

Simply reads/writes the raw value in the configured endianness.

| Type | Payload Size |
|------|-------------|
| tag_byte | 1 byte |
| tag_short | 2 bytes |
| tag_int | 4 bytes |
| tag_long | 8 bytes |
| tag_float | 4 bytes |
| tag_double | 8 bytes |

### tag_string

Payload: 2-byte length + UTF-8 data.

```cpp
void tag_string::read_payload(io::stream_reader& reader)
{
    val = reader.read_string();
}

void tag_string::write_payload(io::stream_writer& writer) const
{
    writer.write_string(val);
}
```

### tag_array<T>

Payload: 4-byte signed length + elements.

Specialized for different element types:

**tag_byte_array** (int8_t) — raw block read/write:
```cpp
// Specialization for int8_t (byte array)
void tag_array<int8_t>::read_payload(io::stream_reader& reader)
{
    int32_t length;
    reader.read_num(length);
    if (length < 0)
        reader.get_istr().setstate(std::ios::failbit);
    if (!reader.get_istr())
        throw io::input_error("Error reading length of tag_byte_array");
    data.resize(length);
    reader.get_istr().read(reinterpret_cast<char*>(data.data()), length);
    if (!reader.get_istr())
        throw io::input_error("Error reading tag_byte_array");
}
```

**tag_long_array** (int64_t) — element-by-element:
```cpp
// Specialization for int64_t (long array)
void tag_array<int64_t>::read_payload(io::stream_reader& reader)
{
    int32_t length;
    reader.read_num(length);
    if (length < 0)
        reader.get_istr().setstate(std::ios::failbit);
    if (!reader.get_istr())
        throw io::input_error("Error reading length of tag_long_array");
    data.clear();
    data.reserve(length);
    for (int32_t i = 0; i < length; ++i) {
        int64_t val;
        reader.read_num(val);
        data.push_back(val);
    }
    if (!reader.get_istr())
        throw io::input_error("Error reading tag_long_array");
}
```

**Generic T** (int32_t for tag_int_array):
```cpp
template <class T>
void tag_array<T>::read_payload(io::stream_reader& reader)
{
    int32_t length;
    reader.read_num(length);
    if (length < 0)
        reader.get_istr().setstate(std::ios::failbit);
    if (!reader.get_istr())
        throw io::input_error("Error reading length of tag_array");
    data.clear();
    data.reserve(length);
    for (int32_t i = 0; i < length; ++i) {
        T val;
        reader.read_num(val);
        data.push_back(val);
    }
    if (!reader.get_istr())
        throw io::input_error("Error reading tag_array");
}
```

### tag_compound

Payload: sequence of complete named tags, terminated by `tag_type::End` (single 0 byte):

```cpp
void tag_compound::read_payload(io::stream_reader& reader)
{
    clear();
    std::pair<std::string, std::unique_ptr<tag>> entry;
    while ((entry = reader.read_tag()).second)
        tags.emplace(std::move(entry.first), std::move(entry.second));
    if (!reader.get_istr())
        throw io::input_error("Error reading tag_compound");
}

void tag_compound::write_payload(io::stream_writer& writer) const
{
    for (const auto& pair : tags) {
        writer.write_type(pair.second.get_type());
        writer.write_string(pair.first);
        pair.second.get().write_payload(writer);
    }
    writer.write_type(tag_type::End);
}
```

### tag_list

Payload: 1-byte element type + 4-byte signed length + element payloads (without type bytes):

(See the [list-tags.md](list-tags.md) document for the full implementation.)

---

## Depth Tracking

`stream_reader` tracks recursive depth to prevent stack overflow from maliciously crafted NBT data with deeply nested compounds or lists:

```cpp
static const unsigned int MAX_DEPTH = 1024;
```

Each call to `read_payload()` increments `depth_`, and decrements on return. If `depth_` exceeds 1024, an `io::input_error` is thrown.

This is critical for security — without depth limits, a crafted file with thousands of nested compounds could cause a stack overflow.

---

## Endianness

Both `stream_reader` and `stream_writer` take an `endian::endian` parameter:

| Value | Use Case |
|-------|----------|
| `endian::big` | Java Edition NBT (default, per Minecraft specification) |
| `endian::little` | Bedrock Edition NBT |

The endianness affects all numeric reads/writes (lengths, primitive values, etc.) but not single bytes (type, byte values).

---

## Usage Examples

### Reading a File

```cpp
#include <nbt_tags.h>
#include <io/stream_reader.h>
#include <fstream>

std::ifstream file("level.dat", std::ios::binary);
auto result = nbt::io::read_compound(file);

std::string name = result.first;              // Root tag name
tag_compound& root = result.second->as<tag_compound>();

int32_t version = static_cast<int32_t>(root.at("version"));
```

### Reading with zlib Decompression

```cpp
#include <io/izlibstream.h>

std::ifstream file("level.dat", std::ios::binary);
zlib::izlibstream zs(file);
auto result = nbt::io::read_compound(zs);
```

### Writing a File

```cpp
#include <io/stream_writer.h>
#include <fstream>

tag_compound root{
    {"Data", tag_compound{
        {"version", int32_t(19133)},
        {"LevelName", std::string("My World")}
    }}
};

std::ofstream file("level.dat", std::ios::binary);
nbt::io::write_tag("", root, file);
```

### Writing with zlib Compression

```cpp
#include <io/ozlibstream.h>

std::ofstream file("level.dat", std::ios::binary);
zlib::ozlibstream zs(file);
nbt::io::write_tag("", root, zs);
zs.close();
```

### Little-Endian (Bedrock)

```cpp
auto result = nbt::io::read_compound(file, endian::endian::little);
nbt::io::write_tag("", root, file, endian::endian::little);
```

### Roundtrip Test

```cpp
// Write
std::stringstream ss;
nbt::io::write_tag("test", original_root, ss);

// Read back
ss.seekg(0);
auto [name, tag] = nbt::io::read_tag(ss);
assert(name == "test");
assert(*tag == original_root);
```

---

## Wire Format Summary

```
Named Tag:
  [type: 1 byte] [name_length: 2 bytes] [name: N bytes] [payload: variable]

Compound Payload:
  [child_tag_1] [child_tag_2] ... [End: 0x00]

List Payload:
  [element_type: 1 byte] [length: 4 bytes] [payload_1] [payload_2] ...

String Payload:
  [length: 2 bytes] [data: N bytes, UTF-8]

Array Payload (Byte/Int/Long):
  [length: 4 bytes] [element_1] [element_2] ...

Primitive Payloads:
  Byte: 1 byte
  Short: 2 bytes
  Int: 4 bytes
  Long: 8 bytes
  Float: 4 bytes (IEEE 754)
  Double: 8 bytes (IEEE 754)
```

All multi-byte values use the configured endianness (big-endian by default).
