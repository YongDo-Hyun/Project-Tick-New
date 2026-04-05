# json4cpp — Testing

## Test Framework

The test suite uses **doctest** (a single-header C++ testing framework).
Tests are located in `json4cpp/tests/src/` with one file per feature area.

## Test File Naming

All test files follow the pattern `unit-<feature>.cpp`:

| File | Covers |
|---|---|
| `unit-allocator.cpp` | Custom allocator support |
| `unit-alt-string.cpp` | Alternative string types |
| `unit-bson.cpp` | BSON serialization/deserialization |
| `unit-bjdata.cpp` | BJData format |
| `unit-capacity.cpp` | `size()`, `empty()`, `max_size()` |
| `unit-cbor.cpp` | CBOR format |
| `unit-class_const_iterator.cpp` | `const_iterator` behavior |
| `unit-class_iterator.cpp` | `iterator` behavior |
| `unit-class_lexer.cpp` | Lexer token scanning |
| `unit-class_parser.cpp` | Parser behavior |
| `unit-comparison.cpp` | Comparison operators |
| `unit-concepts.cpp` | Concept/type trait checks |
| `unit-constructor1.cpp` | Constructor overloads (part 1) |
| `unit-constructor2.cpp` | Constructor overloads (part 2) |
| `unit-convenience.cpp` | Convenience methods (`type_name`, etc.) |
| `unit-conversions.cpp` | Type conversions |
| `unit-deserialization.cpp` | Parsing from various sources |
| `unit-diagnostics.cpp` | `JSON_DIAGNOSTICS` mode |
| `unit-element_access1.cpp` | `operator[]`, `at()` (part 1) |
| `unit-element_access2.cpp` | `value()`, `front()`, `back()` (part 2) |
| `unit-hash.cpp` | `std::hash` specialization |
| `unit-inspection.cpp` | `is_*()`, `type()` methods |
| `unit-items.cpp` | `items()` iteration proxy |
| `unit-iterators1.cpp` | Forward iterators |
| `unit-iterators2.cpp` | Reverse iterators |
| `unit-json_patch.cpp` | JSON Patch (RFC 6902) |
| `unit-json_pointer.cpp` | JSON Pointer (RFC 6901) |
| `unit-large_json.cpp` | Large document handling |
| `unit-merge_patch.cpp` | Merge Patch (RFC 7396) |
| `unit-meta.cpp` | Library metadata |
| `unit-modifiers.cpp` | `push_back()`, `insert()`, `erase()`, etc. |
| `unit-msgpack.cpp` | MessagePack format |
| `unit-ordered_json.cpp` | `ordered_json` behavior |
| `unit-ordered_map.cpp` | `ordered_map` internals |
| `unit-pointer_access.cpp` | `get_ptr()` |
| `unit-readme.cpp` | Examples from README |
| `unit-reference_access.cpp` | `get_ref()` |
| `unit-regression1.cpp` | Regression tests (part 1) |
| `unit-regression2.cpp` | Regression tests (part 2) |
| `unit-serialization.cpp` | `dump()`, stream output |
| `unit-testsuites.cpp` | External test suites (JSONTestSuite, etc.) |
| `unit-to_chars.cpp` | Float-to-string conversion |
| `unit-ubjson.cpp` | UBJSON format |
| `unit-udt.cpp` | User-defined type conversions |
| `unit-udt_macro.cpp` | `NLOHMANN_DEFINE_TYPE_*` macros |
| `unit-unicode1.cpp` | Unicode handling (part 1) |
| `unit-unicode2.cpp` | Unicode handling (part 2) |
| `unit-unicode3.cpp` | Unicode handling (part 3) |
| `unit-unicode4.cpp` | Unicode handling (part 4) |
| `unit-unicode5.cpp` | Unicode handling (part 5) |
| `unit-wstring.cpp` | Wide string support |

## CMake Configuration

From `tests/CMakeLists.txt`:

```cmake
# Test data files
file(GLOB_RECURSE JSON_TEST_DATA_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/data/*.json"
     "${CMAKE_CURRENT_SOURCE_DIR}/data/*.cbor"
     "${CMAKE_CURRENT_SOURCE_DIR}/data/*.msgpack"
     "${CMAKE_CURRENT_SOURCE_DIR}/data/*.bson"
     "${CMAKE_CURRENT_SOURCE_DIR}/data/*.ubjson"
     "${CMAKE_CURRENT_SOURCE_DIR}/data/*.bjdata")

# Each unit-*.cpp compiles to its own test executable
```

Key CMake options affecting tests:

| Option | Effect |
|---|---|
| `JSON_BuildTests` | Enable/disable test building |
| `JSON_Diagnostics` | Build tests with `JSON_DIAGNOSTICS=1` |
| `JSON_Diagnostic_Positions` | Build tests with position tracking |
| `JSON_MultipleHeaders` | Use multi-header include paths |
| `JSON_ImplicitConversions` | Test with implicit conversions on/off |
| `JSON_DisableEnumSerialization` | Test with enum serialization off |
| `JSON_GlobalUDLs` | Test with global UDLs on/off |

## Building Tests

```bash
cd json4cpp
mkdir build && cd build

# Configure with tests enabled
cmake .. -DJSON_BuildTests=ON

# Build
cmake --build .

# Run all tests
ctest --output-on-failure

# Run a specific test
./tests/test-unit-json_pointer
```

## Test Structure

Tests use doctest's `TEST_CASE` and `SECTION` macros:

```cpp
#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

TEST_CASE("element access") {
    SECTION("array") {
        json j = {1, 2, 3};

        SECTION("operator[]") {
            CHECK(j[0] == 1);
            CHECK(j[1] == 2);
            CHECK(j[2] == 3);
        }

        SECTION("at()") {
            CHECK(j.at(0) == 1);
            CHECK_THROWS_AS(j.at(5), json::out_of_range);
        }
    }

    SECTION("object") {
        json j = {{"key", "value"}};

        CHECK(j["key"] == "value");
        CHECK(j.at("key") == "value");
        CHECK_THROWS_AS(j.at("missing"), json::out_of_range);
    }
}
```

### Common Assertions

| Macro | Purpose |
|---|---|
| `CHECK(expr)` | Value assertion (non-fatal) |
| `REQUIRE(expr)` | Value assertion (fatal) |
| `CHECK_THROWS_AS(expr, type)` | Exception type assertion |
| `CHECK_THROWS_WITH_AS(expr, msg, type)` | Exception message + type |
| `CHECK_NOTHROW(expr)` | No exception assertion |

## Test Data

The `tests/data/` directory contains JSON files for conformance testing:
- Input from JSONTestSuite (parsing edge cases)
- Binary format test vectors (CBOR, MessagePack, UBJSON, BSON, BJData)
- Unicode test cases
- Large nested structures

## Running Specific Tests

doctest supports command-line filtering:

```bash
# Run tests matching a substring
./test-unit-json_pointer -tc="JSON pointer"

# List all test cases
./test-unit-json_pointer -ltc

# Run with verbose output
./test-unit-json_pointer -s
```

## Continuous Integration

Tests are run across multiple compilers and platforms via CI (see `ci/`
directory). The `ci/supportedBranches.js` file lists which branches are
tested. The test matrix covers:
- GCC, Clang, MSVC
- C++11 through C++20
- Various option combinations (diagnostics, implicit conversions, etc.)
