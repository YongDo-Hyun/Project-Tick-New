# Endian Handling

## Overview

The `endian` namespace provides byte-order conversion for reading and writing multi-byte numeric values in big-endian or little-endian format. This is the lowest layer of the I/O system, called by `stream_reader::read_num()` and `stream_writer::write_num()`.

Defined in `include/endian_str.h`, implemented in `src/endian_str.cpp`.

---

## Endianness Enum

```cpp
namespace endian {

enum class endian
{
    big,
    little
};

}
```

- `endian::big` — Most significant byte first. Default for Java Edition NBT (per the Minecraft specification).
- `endian::little` — Least significant byte first. Used by Bedrock Edition NBT.

---

## Public API

### Read Functions

```cpp
namespace endian {

template <class T>
void read(std::istream& is, T& x, endian e);

void read_little(std::istream& is, uint8_t& x);
void read_little(std::istream& is, int8_t& x);
void read_little(std::istream& is, uint16_t& x);
void read_little(std::istream& is, int16_t& x);
void read_little(std::istream& is, uint32_t& x);
void read_little(std::istream& is, int32_t& x);
void read_little(std::istream& is, uint64_t& x);
void read_little(std::istream& is, int64_t& x);
void read_little(std::istream& is, float& x);
void read_little(std::istream& is, double& x);

void read_big(std::istream& is, uint8_t& x);
void read_big(std::istream& is, int8_t& x);
void read_big(std::istream& is, uint16_t& x);
void read_big(std::istream& is, int16_t& x);
void read_big(std::istream& is, uint32_t& x);
void read_big(std::istream& is, int32_t& x);
void read_big(std::istream& is, uint64_t& x);
void read_big(std::istream& is, int64_t& x);
void read_big(std::istream& is, float& x);
void read_big(std::istream& is, double& x);

}
```

### Write Functions

```cpp
namespace endian {

template <class T>
void write(std::ostream& os, T x, endian e);

void write_little(std::ostream& os, uint8_t x);
void write_little(std::ostream& os, int8_t x);
void write_little(std::ostream& os, uint16_t x);
void write_little(std::ostream& os, int16_t x);
void write_little(std::ostream& os, uint32_t x);
void write_little(std::ostream& os, int32_t x);
void write_little(std::ostream& os, uint64_t x);
void write_little(std::ostream& os, int64_t x);
void write_little(std::ostream& os, float x);
void write_little(std::ostream& os, double x);

void write_big(std::ostream& os, uint8_t x);
void write_big(std::ostream& os, int8_t x);
void write_big(std::ostream& os, uint16_t x);
void write_big(std::ostream& os, int16_t x);
void write_big(std::ostream& os, uint32_t x);
void write_big(std::ostream& os, int32_t x);
void write_big(std::ostream& os, uint64_t x);
void write_big(std::ostream& os, int64_t x);
void write_big(std::ostream& os, float x);
void write_big(std::ostream& os, double x);

}
```

---

## Template Dispatch

The `read()` and `write()` templates dispatch to the correct endian-specific function:

```cpp
template <class T>
void read(std::istream& is, T& x, endian e)
{
    switch (e) {
        case endian::big:    read_big(is, x); break;
        case endian::little: read_little(is, x); break;
    }
}

template <class T>
void write(std::ostream& os, T x, endian e)
{
    switch (e) {
        case endian::big:    write_big(os, x); break;
        case endian::little: write_little(os, x); break;
    }
}
```

This is called by `stream_reader` and `stream_writer`:

```cpp
// In stream_reader
template <class T> void read_num(T& x)
{
    endian::read(is, x, endian);
}

// In stream_writer
template <class T> void write_num(T x)
{
    endian::write(os, x, endian);
}
```

---

## Implementation Details

### Static Assertions

The implementation begins with compile-time checks:

```cpp
static_assert(CHAR_BIT == 8,    "Assumes 8-bit bytes");
static_assert(sizeof(float) == 4,  "Assumes 32-bit float");
static_assert(sizeof(double) == 8, "Assumes 64-bit double");
```

### Single-Byte Types

For `int8_t` and `uint8_t`, endianness is irrelevant — the byte is read/written directly:

```cpp
void read_little(std::istream& is, uint8_t& x)
{
    x = is.get();
}

void write_little(std::ostream& os, uint8_t x)
{
    os.put(x);
}

// Same for read_big/write_big
```

### Multi-Byte Integer Types

Bytes are read/written individually and assembled in the correct order.

**Big-endian read (most significant byte first):**
```cpp
void read_big(std::istream& is, uint16_t& x)
{
    uint8_t bytes[2];
    is.read(reinterpret_cast<char*>(bytes), 2);
    x = static_cast<uint16_t>(bytes[0]) << 8
      | static_cast<uint16_t>(bytes[1]);
}

void read_big(std::istream& is, uint32_t& x)
{
    uint8_t bytes[4];
    is.read(reinterpret_cast<char*>(bytes), 4);
    x = static_cast<uint32_t>(bytes[0]) << 24
      | static_cast<uint32_t>(bytes[1]) << 16
      | static_cast<uint32_t>(bytes[2]) << 8
      | static_cast<uint32_t>(bytes[3]);
}

void read_big(std::istream& is, uint64_t& x)
{
    uint8_t bytes[8];
    is.read(reinterpret_cast<char*>(bytes), 8);
    x = static_cast<uint64_t>(bytes[0]) << 56
      | static_cast<uint64_t>(bytes[1]) << 48
      | static_cast<uint64_t>(bytes[2]) << 40
      | static_cast<uint64_t>(bytes[3]) << 32
      | static_cast<uint64_t>(bytes[4]) << 24
      | static_cast<uint64_t>(bytes[5]) << 16
      | static_cast<uint64_t>(bytes[6]) << 8
      | static_cast<uint64_t>(bytes[7]);
}
```

**Little-endian read (least significant byte first):**
```cpp
void read_little(std::istream& is, uint16_t& x)
{
    uint8_t bytes[2];
    is.read(reinterpret_cast<char*>(bytes), 2);
    x = static_cast<uint16_t>(bytes[1]) << 8
      | static_cast<uint16_t>(bytes[0]);
}
```

**Big-endian write:**
```cpp
void write_big(std::ostream& os, uint16_t x)
{
    os.put(static_cast<char>(x >> 8));
    os.put(static_cast<char>(x));
}

void write_big(std::ostream& os, uint32_t x)
{
    os.put(static_cast<char>(x >> 24));
    os.put(static_cast<char>(x >> 16));
    os.put(static_cast<char>(x >> 8));
    os.put(static_cast<char>(x));
}
```

**Little-endian write:**
```cpp
void write_little(std::ostream& os, uint16_t x)
{
    os.put(static_cast<char>(x));
    os.put(static_cast<char>(x >> 8));
}
```

### Signed Types

Signed integers delegate to unsigned via `reinterpret_cast`:

```cpp
void read_big(std::istream& is, int16_t& x)
{
    read_big(is, reinterpret_cast<uint16_t&>(x));
}

void write_big(std::ostream& os, int16_t x)
{
    write_big(os, static_cast<uint16_t>(x));
}
```

This works because the bit patterns are identical — only interpretation differs.

### Floating-Point Types

Floats and doubles use `memcpy` to convert between floating-point and integer representations, avoiding undefined behavior from type-punning casts:

```cpp
void read_big(std::istream& is, float& x)
{
    uint32_t tmp;
    read_big(is, tmp);
    std::memcpy(&x, &tmp, sizeof(x));
}

void write_big(std::ostream& os, float x)
{
    uint32_t tmp;
    std::memcpy(&tmp, &x, sizeof(tmp));
    write_big(os, tmp);
}

void read_big(std::istream& is, double& x)
{
    uint64_t tmp;
    read_big(is, tmp);
    std::memcpy(&x, &tmp, sizeof(x));
}

void write_big(std::ostream& os, double x)
{
    uint64_t tmp;
    std::memcpy(&tmp, &x, sizeof(tmp));
    write_big(os, tmp);
}
```

The `memcpy` approach:
- Is defined behavior in C++11 (unlike `reinterpret_cast` between float/int, which is UB)
- Assumes IEEE 754 representation (verified by `static_assert(sizeof(float) == 4)`)
- Is typically optimized by the compiler to a no-op or register move

---

## Byte Layout Reference

### Big-Endian (Java Edition Default)

```
Value: 0x12345678 (int32_t)
Memory: [0x12] [0x34] [0x56] [0x78]
         MSB                    LSB

Value: 3.14f (float, IEEE 754: 0x4048F5C3)
Memory: [0x40] [0x48] [0xF5] [0xC3]
```

### Little-Endian (Bedrock Edition)

```
Value: 0x12345678 (int32_t)
Memory: [0x78] [0x56] [0x34] [0x12]
         LSB                    MSB

Value: 3.14f (float, IEEE 754: 0x4048F5C3)
Memory: [0xC3] [0xF5] [0x48] [0x40]
```

---

## Supported Types

| C++ Type | Size | NBT Use |
|----------|------|---------|
| `int8_t` / `uint8_t` | 1 byte | tag_byte, type bytes |
| `int16_t` / `uint16_t` | 2 bytes | tag_short, string lengths |
| `int32_t` / `uint32_t` | 4 bytes | tag_int, array/list lengths |
| `int64_t` / `uint64_t` | 8 bytes | tag_long |
| `float` | 4 bytes | tag_float |
| `double` | 8 bytes | tag_double |

---

## Design Rationale

### Why Not Use System Endianness Detection?

The implementation always performs explicit byte-by-byte construction rather than detecting the host endianness and potentially passing through. This approach:

1. **Portable**: Works correctly on any architecture (big-endian, little-endian, or mixed)
2. **Simple**: No preprocessor conditionals or platform detection
3. **Correct**: No alignment issues since bytes are assembled individually
4. **Predictable**: Same code path on all platforms

### Why memcpy for Floats?

C++ standards do not guarantee that `reinterpret_cast<uint32_t&>(float_val)` produces defined behavior (strict aliasing violation). `memcpy` is the standard-sanctioned way to perform type punning between unrelated types, and modern compilers optimize it to equivalent machine code.
