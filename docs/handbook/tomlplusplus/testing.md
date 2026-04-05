# toml++ — Testing

## Overview

toml++ uses the **Catch2** testing framework. Tests are organized in `tests/` and built via Meson. The test suite includes unit tests for every major feature, TOML specification conformance tests, and third-party test suites.

---

## Test Framework

### Catch2

- Used as the test runner and assertion library
- Can be vendored (`extern/Catch2/`) or found as a system dependency
- Tests use `TEST_CASE`, `SECTION`, `REQUIRE`, `CHECK` macros

### Build Configuration

Tests are built with Meson. The test build options from `meson_options.txt`:

```
option('build_tests', type: 'boolean', value: false)
option('use_vendored_libs', type: 'boolean', value: true)
```

Build and run tests:

```bash
meson setup build -Dbuild_tests=true
meson compile -C build
meson test -C build
```

---

## Test File Organization

From `tests/meson.build`, the test suite consists of:

### Conformance Tests

Third-party test suites that validate the parser against the TOML specification:

| Test Suite | Files | Description |
|-----------|-------|-------------|
| BurntSushi (valid) | `conformance_burntsushi_valid.cpp` | Validates that valid TOML parses correctly |
| BurntSushi (invalid) | `conformance_burntsushi_invalid.cpp` | Validates that invalid TOML is rejected |
| iarna (valid) | `conformance_iarna_valid.cpp` | Additional valid TOML test cases |
| iarna (invalid) | `conformance_iarna_invalid.cpp` | Additional invalid TOML test cases |

Test data files are in `tests/` subdirectories corresponding to each third-party suite.

### Parsing Tests

Unit tests for the parser:

| File | Content |
|------|---------|
| `parsing_arrays.cpp` | Array parsing edge cases |
| `parsing_booleans.cpp` | Boolean value parsing |
| `parsing_comments.cpp` | Comment handling |
| `parsing_dates_and_times.cpp` | Date/time value parsing |
| `parsing_floats.cpp` | Float parsing (inf, nan, precision) |
| `parsing_integers.cpp` | Integer parsing (hex, oct, bin, overflow) |
| `parsing_key_value_pairs.cpp` | Key-value pair syntax |
| `parsing_spec_example.cpp` | TOML spec example document |
| `parsing_strings.cpp` | All 4 string types, escape sequences |
| `parsing_tables.cpp` | Standard and inline tables |

### Manipulation Tests

Tests for programmatic construction and modification:

| File | Content |
|------|---------|
| `manipulating_arrays.cpp` | Array push_back, erase, flatten, etc. |
| `manipulating_tables.cpp` | Table insert, emplace, merge, etc. |
| `manipulating_values.cpp` | Value construction, assignment, flags |
| `manipulating_parse_result.cpp` | parse_result access patterns |

### Formatter Tests

| File | Content |
|------|---------|
| `formatters.cpp` | TOML, JSON, and YAML formatter output |

### Path Tests

| File | Content |
|------|---------|
| `path.cpp` | Path parsing, navigation, at_path() |

### Other Tests

| File | Content |
|------|---------|
| `at_path.cpp` | at_path() function specifically |
| `for_each.cpp` | for_each() visitor pattern |
| `user_feedback.cpp` | Tests from user-reported issues |
| `windows_compat.cpp` | Windows wstring compatibility |
| `using_iterators.cpp` | Iterator usage patterns |
| `main.cpp` | Catch2 main entry point |
| `tests.hpp` | Shared test utilities and macros |

---

## Running Tests

### Full Test Suite

```bash
cd tomlplusplus
meson setup build -Dbuild_tests=true
meson compile -C build
meson test -C build
```

### Verbose Output

```bash
meson test -C build -v
```

### Running Specific Tests

Catch2 allows filtering by test name:

```bash
./build/tests/toml_test "parsing integers"
./build/tests/toml_test "[arrays]"
```

### Exception / No-Exception Modes

Tests are compiled in both modes when possible:

```bash
# With exceptions (default)
meson setup build_exc -Dbuild_tests=true

# Without exceptions
meson setup build_noexc -Dbuild_tests=true -Dcpp_eh=none
```

---

## Test Patterns

### Parsing Roundtrip

A common pattern: parse TOML, verify values, re-serialize, verify output:

```cpp
TEST_CASE("integers - hex")
{
    auto tbl = toml::parse("val = 0xFF");
    CHECK(tbl["val"].value<int64_t>() == 255);
    CHECK(tbl["val"].as_integer()->flags() == toml::value_flags::format_as_hexadecimal);
}
```

### Invalid Input Rejection

```cpp
TEST_CASE("invalid - unterminated string")
{
    CHECK_THROWS_AS(toml::parse("val = \"unterminated"), toml::parse_error);
}
```

Or without exceptions:

```cpp
TEST_CASE("invalid - unterminated string")
{
    auto result = toml::parse("val = \"unterminated");
    CHECK(!result);
    CHECK(result.error().description().find("unterminated") != std::string_view::npos);
}
```

### Manipulation Verification

```cpp
TEST_CASE("array - push_back")
{
    toml::array arr;
    arr.push_back(1);
    arr.push_back("two");
    arr.push_back(3.0);

    REQUIRE(arr.size() == 3);
    CHECK(arr[0].value<int64_t>() == 1);
    CHECK(arr[1].value<std::string_view>() == "two");
    CHECK(arr[2].value<double>() == 3.0);
}
```

---

## Adding New Tests

1. Create a `.cpp` file in `tests/`
2. Include `"tests.hpp"` for common utilities
3. Add the file to the test source list in `tests/meson.build`
4. Write `TEST_CASE` blocks using Catch2 macros
5. Rebuild and run

```cpp
// tests/my_feature.cpp
#include "tests.hpp"

TEST_CASE("my feature - basic behavior")
{
    auto tbl = toml::parse(R"(key = "value")");
    REQUIRE(tbl["key"].value<std::string_view>() == "value");
}
```

---

## Related Documentation

- [building.md](building.md) — Build system setup
- [code-style.md](code-style.md) — Code conventions
- [parsing.md](parsing.md) — Parser being tested
