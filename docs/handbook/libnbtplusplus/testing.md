# Testing

## Overview

libnbt++ uses the **CxxTest** testing framework. Tests are defined as C++ header files with test classes inheriting from `CxxTest::TestSuite`. The test suite covers all tag types, I/O operations, endian conversion, zlib streams, and value semantics.

Build configuration is in `test/CMakeLists.txt`.

---

## Build Configuration

```cmake
# test/CMakeLists.txt
if(NOT (UNIX AND (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|i[3-6]86")))
    message(WARNING "Tests are only supported on Linux x86/x86_64")
    return()
endif()

find_package(CxxTest REQUIRED)
```

Tests are **Linux x86/x86_64 only** due to the use of `objcopy` for embedding binary test data.

### CMake Options

Tests are controlled by the `NBT_BUILD_TESTS` option:

```cmake
option(NBT_BUILD_TESTS "Build libnbt++ tests" ON)

if(NBT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
```

### Binary Test Data Embedding

Test data files (e.g., `bigtest.nbt`, `bigtest_uncompr`, `littletest.nbt`) are converted to object files via `objcopy` and linked directly into test executables:

```cmake
set(BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/")

function(add_nbt_test name)
    # ...
    foreach(binfile ${ARGN})
        set(obj "${CMAKE_CURRENT_BINARY_DIR}/${binfile}.o")
        add_custom_command(
            OUTPUT ${obj}
            COMMAND objcopy -I binary -O elf64-x86-64
                -B i386:x86-64 ${binfile} ${obj}
            WORKING_DIRECTORY ${BINARY_DIR}
            DEPENDS ${BINARY_DIR}/${binfile}
        )
        target_sources(${name} PRIVATE ${obj})
    endforeach()
endfunction()
```

The embedded data is accessed via extern symbols declared in `test/data.h`:

```cpp
// test/data.h
#define DECLARE_BINARY(name) \
    extern "C" const char _binary_##name##_start[]; \
    extern "C" const char _binary_##name##_end[];

DECLARE_BINARY(bigtest_nbt)
DECLARE_BINARY(bigtest_uncompr)
DECLARE_BINARY(littletest_nbt)
DECLARE_BINARY(littletest_uncompr)
DECLARE_BINARY(errortest_eof_nbt)
DECLARE_BINARY(errortest_negative_length_nbt)
DECLARE_BINARY(errortest_excessive_depth_nbt)
```

---

## Test Targets

| Target | Source | Tests |
|--------|--------|-------|
| `nbttest` | `test/nbttest.h` | Core tag types, value, compound, list, visitor |
| `endian_str_test` | `test/endian_str_test.h` | Endian read/write roundtrips |
| `read_test` | `test/read_test.h` | stream_reader, big/little endian, errors |
| `write_test` | `test/write_test.h` | stream_writer, payload writing, roundtrips |
| `zlibstream_test` | `test/zlibstream_test.h` | izlibstream, ozlibstream, compression roundtrip |
| `format_test` | `test/format_test.cpp` | JSON text formatting |
| `test_value` | `test/test_value.h` | Value numeric assignment and conversion |

Test registration in CMake:

```cmake
add_nbt_test(nbttest nbttest.h)
add_nbt_test(endian_str_test endian_str_test.h)
add_nbt_test(read_test read_test.h
    bigtest.nbt bigtest_uncompr littletest.nbt littletest_uncompr
    errortest_eof.nbt errortest_negative_length.nbt
    errortest_excessive_depth.nbt)
add_nbt_test(write_test write_test.h
    bigtest_uncompr littletest_uncompr)
if(NBT_HAVE_ZLIB)
    add_nbt_test(zlibstream_test zlibstream_test.h
        bigtest.nbt bigtest_uncompr)
endif()
add_nbt_test(test_value test_value.h)
```

`zlibstream_test` is only built when `NBT_HAVE_ZLIB` is defined.

---

## Test Details

### nbttest.h — Core Functionality

Tests the fundamental tag and value operations:

```cpp
class NbtTest : public CxxTest::TestSuite
{
public:
    void test_tag_primitive();     // Constructors, get/set, implicit conversion
    void test_tag_string();        // String constructors, conversion operators
    void test_tag_compound();      // Insertion, lookup, iteration, has_key()
    void test_tag_list();          // Type enforcement, push_back, of<T>()
    void test_tag_array();         // Vector access, constructors
    void test_value();             // Type erasure, numeric assignment
    void test_visitor();           // Double dispatch, visitor invocation
    void test_equality();          // operator==, operator!= for all types
    void test_clone();             // clone() and move_clone() correctness
    void test_as();                // tag::as<T>() casting, bad_cast
};
```

### read_test.h — Deserialization

Tests `stream_reader` against known binary data:

```cpp
class ReadTest : public CxxTest::TestSuite
{
public:
    void test_read_bigtest();       // Verifies full bigtest.nbt structure
    void test_read_littletest();    // Little-endian variant
    void test_read_bigtest_uncompr(); // Uncompressed big-endian
    void test_read_littletest_uncompr(); // Uncompressed little-endian
    void test_read_eof();           // Truncated data → input_error
    void test_read_negative_length(); // Negative array length → input_error
    void test_read_excessive_depth(); // >1024 nesting → input_error
};
```

The "bigtest" validates a complex nested compound with all tag types — the standard NBT test file from the Minecraft community. Fields verified include:

- `"byteTest"`: `tag_byte` with value 127
- `"shortTest"`: `tag_short` with value 32767
- `"intTest"`: `tag_int` with value 2147483647
- `"longTest"`: `tag_long` with value 9223372036854775807
- `"floatTest"`: `tag_float` with value ~0.498...
- `"doubleTest"`: `tag_double` with value ~0.493...
- `"stringTest"`: UTF-8 string "HELLO WORLD THIS IS A TEST STRING ÅÄÖ!"
- `"byteArrayTest"`: 1000-element byte array
- `"listTest (long)"`: List of 5 longs
- `"listTest (compound)"`: List of compounds
- Nested compound within compound

### write_test.h — Serialization

Tests `stream_writer` and write-then-read roundtrips:

```cpp
class WriteTest : public CxxTest::TestSuite
{
public:
    void test_write_bigtest();      // Write and compare against reference
    void test_write_littletest();   // Little-endian write
    void test_write_payload();      // Individual payload writing
    void test_roundtrip();          // Write → read → compare equality
};
```

### endian_str_test.h — Byte Order

Tests all numeric types through read/write roundtrips in both endiannesses:

```cpp
class EndianStrTest : public CxxTest::TestSuite
{
public:
    void test_read_big();
    void test_read_little();
    void test_write_big();
    void test_write_little();
    void test_roundtrip_big();
    void test_roundtrip_little();
    void test_float_big();
    void test_float_little();
    void test_double_big();
    void test_double_little();
};
```

### zlibstream_test.h — Compression

Tests zlib stream wrappers:

```cpp
class ZlibstreamTest : public CxxTest::TestSuite
{
public:
    void test_inflate_gzip();       // Decompress gzip data
    void test_inflate_zlib();       // Decompress zlib data
    void test_inflate_corrupt();    // Corrupt data → zlib_error
    void test_inflate_eof();        // Truncated data → EOF
    void test_inflate_trailing();   // Data after compressed stream
    void test_deflate_roundtrip();  // Compress → decompress → compare
    void test_deflate_gzip();       // Gzip format output
};
```

### test_value.h — Value Semantics

Tests the `value` class's numeric assignment and conversion:

```cpp
class TestValue : public CxxTest::TestSuite
{
public:
    void test_assign_byte();
    void test_assign_short();
    void test_assign_int();
    void test_assign_long();
    void test_assign_float();
    void test_assign_double();
    void test_assign_widening();    // int8 → int16 → int32 → int64
    void test_assign_narrowing();   // Narrowing disallowed
    void test_assign_string();
};
```

### format_test.cpp — Text Output

A standalone test (not CxxTest) that constructs a compound, serializes it, reads it back, and prints JSON:

```cpp
// Constructs a compound with nested types
// Writes to stringstream, reads back
// Outputs via operator<< (json_formatter)
```

---

## Running Tests

```bash
mkdir build && cd build
cmake .. -DNBT_BUILD_TESTS=ON
cmake --build .
ctest
```

Or run individual tests:

```bash
./test/nbttest
./test/read_test
./test/write_test
./test/endian_str_test
./test/zlibstream_test
./test/test_value
```

---

## Test Data Files

Located in `test/`:

| File | Description |
|------|-------------|
| `bigtest.nbt` | Standard NBT test file, gzip-compressed, big-endian |
| `bigtest_uncompr` | Same as bigtest but uncompressed |
| `littletest.nbt` | Little-endian NBT test file, compressed |
| `littletest_uncompr` | Little-endian, uncompressed |
| `errortest_eof.nbt` | Truncated NBT for error testing |
| `errortest_negative_length.nbt` | NBT with negative array length |
| `errortest_excessive_depth.nbt` | NBT with >1024 nesting levels |

These files are embedded into test binaries via `objcopy` and accessed through the `DECLARE_BINARY` macros in `data.h`.
