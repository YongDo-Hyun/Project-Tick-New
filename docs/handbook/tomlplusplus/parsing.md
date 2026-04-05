# toml++ — Parsing

## Overview

toml++ provides a recursive-descent parser that converts TOML text into a `toml::table` tree of nodes. The parser handles the full TOML v1.0.0 specification, including all string types, numeric formats, date/time values, inline tables, and arrays of tables.

Key entry points are `toml::parse()` and `toml::parse_file()`, declared in `include/toml++/impl/parser.hpp`.

The parser can be disabled entirely via `TOML_ENABLE_PARSER=0`.

---

## Entry Points

### `parse()` — Parse from String

```cpp
// From std::string_view (most common)
parse_result parse(std::string_view doc, std::string_view source_path = {});
parse_result parse(std::string_view doc, std::string&& source_path);

// From std::istream
parse_result parse(std::istream& doc, std::string_view source_path = {});
parse_result parse(std::istream& doc, std::string&& source_path);

// From char8_t (C++20 u8 strings)
parse_result parse(std::u8string_view doc, std::string_view source_path = {});
```

The `source_path` parameter is stored in `source_region` data and appears in error messages. It does not affect parsing behavior.

```cpp
auto result = toml::parse(R"(
    [server]
    host = "localhost"
    port = 8080
)");

// With source path for error messages:
auto result2 = toml::parse(toml_string, "config.toml");
```

### `parse_file()` — Parse from File Path

```cpp
parse_result parse_file(std::string_view file_path);

// Windows wstring overload (when TOML_ENABLE_WINDOWS_COMPAT=1)
parse_result parse_file(std::wstring_view file_path);
```

Reads the entire file and parses it:

```cpp
auto config = toml::parse_file("settings.toml");
auto config2 = toml::parse_file("/etc/myapp/config.toml");
```

---

## parse_result

`parse_result` is the return type from all parse functions. Its behavior depends on whether exceptions are enabled.

Declared in `include/toml++/impl/parse_result.hpp`.

### With Exceptions (`TOML_EXCEPTIONS=1`, the default)

`parse_result` is simply a type alias for `toml::table`:

```cpp
// Effectively:
using parse_result = table;
```

If parsing fails, `toml::parse_error` (derived from `std::runtime_error`) is thrown:

```cpp
try
{
    toml::table config = toml::parse("invalid [[[toml");
}
catch (const toml::parse_error& err)
{
    std::cerr << "Parse error: " << err.description() << "\n";
    std::cerr << "  at: " << err.source() << "\n";
}
```

### Without Exceptions (`TOML_EXCEPTIONS=0`)

`parse_result` is a discriminated union — it holds either a `toml::table` (success) or a `toml::parse_error` (failure):

```cpp
class parse_result
{
  public:
    // Check success
    bool succeeded() const noexcept;
    bool failed() const noexcept;
    explicit operator bool() const noexcept;  // same as succeeded()

    // Access the table (success)
    table& get() & noexcept;
    const table& get() const& noexcept;
    table&& get() && noexcept;

    table& operator*() & noexcept;
    table* operator->() noexcept;

    // Access the error (failure)
    const parse_error& error() const& noexcept;

    // Implicit conversion to table& (success only)
    operator table&() noexcept;
    operator const table&() const noexcept;
};
```

Usage pattern:

```cpp
auto result = toml::parse("...");

if (result)
{
    // Success — use the table
    toml::table& config = result;
    auto val = config["key"].value_or("default"sv);
}
else
{
    // Failure — inspect the error
    std::cerr << "Error: " << result.error().description() << "\n";
    std::cerr << "  at " << result.error().source() << "\n";
}
```

### Internal Storage (No-Exceptions Mode)

`parse_result` uses aligned storage to hold either type:

```cpp
// Simplified internal layout:
alignas(table) mutable unsigned char storage_[sizeof(table)];
bool err_;
parse_error err_val_;  // only when err_ == true
```

The `table` is placement-new'd into `storage_` on success. On failure, `err_val_` is populated instead.

---

## parse_error

Declared in `include/toml++/impl/parse_error.hpp`.

### With Exceptions

```cpp
class parse_error : public std::runtime_error
{
  public:
    std::string_view description() const noexcept;
    const source_region& source() const noexcept;

    // what() returns the description
};
```

### Without Exceptions

```cpp
class parse_error
{
  public:
    std::string_view description() const noexcept;
    const source_region& source() const noexcept;

    // Streaming
    friend std::ostream& operator<<(std::ostream&, const parse_error&);
};
```

### Error Information

```cpp
auto result = toml::parse("x = [1, 2,, 3]", "test.toml");

if (!result)
{
    const auto& err = result.error();

    // Human-readable description
    std::cout << err.description() << "\n";
    // e.g., "Unexpected character ',' (U+002C) in array"

    // Source location
    const auto& src = err.source();
    std::cout << "File: " << *src.path << "\n";    // "test.toml"
    std::cout << "Line: " << src.begin.line << "\n"; // 1
    std::cout << "Col:  " << src.begin.column << "\n"; // 11

    // Full error with location (via operator<<)
    std::cout << err << "\n";
    // "Unexpected character ',' ... at line 1, column 11 of 'test.toml'"
}
```

---

## Source Tracking

### `source_position`

```cpp
struct source_position
{
    source_index line;     // 1-based line number
    source_index column;   // 1-based column number (byte offset, not codepoint)

    explicit operator bool() const noexcept;  // true if line > 0

    friend bool operator==(const source_position&, const source_position&) noexcept;
    friend bool operator!=(const source_position&, const source_position&) noexcept;
    friend bool operator< (const source_position&, const source_position&) noexcept;
    friend bool operator<=(const source_position&, const source_position&) noexcept;
};
```

`source_index` is typically `uint32_t` (or `uint16_t` on small builds via `TOML_SMALL_INT_TYPE`).

### `source_region`

```cpp
struct source_region
{
    source_position begin;                      // start of the element
    source_position end;                        // end of the element
    source_path_ptr path;                       // shared_ptr<const std::string>
};
```

Every `node` in the parsed tree carries a `source_region`:

```cpp
auto tbl = toml::parse(R"(
    name = "Alice"
    age = 30
)", "config.toml");

const auto& src = tbl["name"].node()->source();
std::cout << "Defined at "
          << *src.path << ":"
          << src.begin.line << ":"
          << src.begin.column << "\n";
// "Defined at config.toml:2:5"
```

`source_path_ptr` is `std::shared_ptr<const std::string>`, shared among all nodes parsed from the same file.

---

## Stream Parsing

Parsing from `std::istream` allows reading from any stream source:

```cpp
#include <fstream>

std::ifstream file("config.toml");
auto config = toml::parse(file, "config.toml");

// From stringstream
std::istringstream ss(R"(key = "value")");
auto tbl = toml::parse(ss);
```

The stream is consumed entirely during parsing.

---

## UTF-8 Handling During Parsing

The parser expects UTF-8 encoded input. It handles:

- **BOM stripping**: A leading UTF-8 BOM (`0xEF 0xBB 0xBF`) is silently consumed
- **Multi-byte sequences**: Bare keys, strings, and comments can contain Unicode
- **Escape sequences in strings**: `\uXXXX` and `\UXXXXXXXX` are decoded
- **Validation**: Invalid UTF-8 sequences are rejected with parse errors
- **Non-characters and surrogates**: Rejected as per the TOML specification

### char8_t Support (C++20)

```cpp
auto tbl = toml::parse(u8R"(
    greeting = "Hello, 世界"
)"sv);
```

The `char8_t` overloads allow passing C++20 UTF-8 string literals directly.

---

## Windows Compatibility

When `TOML_ENABLE_WINDOWS_COMPAT=1` (default on Windows):

```cpp
// Accept wstring file paths (converted to UTF-8 internally)
auto config = toml::parse_file(L"C:\\config\\settings.toml");

// Wide string values can be used with value()
auto path = config["path"].value<std::wstring>();
```

---

## Parser Configuration Macros

| Macro | Default | Effect |
|-------|---------|--------|
| `TOML_ENABLE_PARSER` | `1` | Set to `0` to remove the parser entirely (serialize-only builds) |
| `TOML_EXCEPTIONS` | Auto-detected | Controls exception vs. return-code error handling |
| `TOML_UNRELEASED_FEATURES` | `0` | Enable parsing of TOML features not yet in a released spec |
| `TOML_ENABLE_WINDOWS_COMPAT` | `1` on Windows | Enable wstring/wchar_t overloads |

---

## Parsing Specific TOML Features

### Strings

```toml
basic = "hello\nworld"           # basic (escape sequences)
literal = 'no \escapes'          # literal (no escapes)
multi_basic = """
multiline
string"""                        # multi-line basic
multi_literal = '''
multiline
literal'''                       # multi-line literal
```

### Numbers

```toml
int_dec = 42
int_hex = 0xFF
int_oct = 0o755
int_bin = 0b11010110
float_std = 3.14
float_exp = 1e10
float_inf = inf
float_nan = nan
underscore = 1_000_000
```

The parser records the integer format in `value_flags`:
- `0xFF` → `value_flags::format_as_hexadecimal`
- `0o755` → `value_flags::format_as_octal`
- `0b1010` → `value_flags::format_as_binary`

### Inline Tables

```toml
point = { x = 1, y = 2 }         # single-line inline table
```

Parsed as a `toml::table` with `is_inline()` returning `true`.

### Arrays of Tables

```toml
[[products]]
name = "Hammer"
price = 9.99

[[products]]
name = "Nail"
price = 0.05
```

Parsed as `table["products"]` → `array` containing two `table` nodes.

---

## Error Recovery

The parser does **not** attempt error recovery. Upon encountering the first error, parsing stops and the error is returned (or thrown). This design ensures:

1. No partially-parsed trees with missing data
2. Clear, unambiguous error messages
3. The error source points to the exact location of the problem

---

## Thread Safety

- Parsing is **thread-safe**: multiple threads can call `parse()` concurrently with different inputs
- The parser uses no global state
- The returned `parse_result` / `table` is independent and owned by the caller

---

## Complete Example

```cpp
#include <toml++/toml.hpp>
#include <iostream>
#include <fstream>

int main()
{
    // --- Parse from string ---
    auto config = toml::parse(R"(
        [database]
        server = "192.168.1.1"
        ports = [8001, 8001, 8002]
        enabled = true
        connection_max = 5000

        [database.credentials]
        username = "admin"
        password_file = "/etc/db/pass"
    )");

#if TOML_EXCEPTIONS
    // With exceptions, config is a toml::table directly
    auto server = config["database"]["server"].value_or(""sv);
    std::cout << "Server: " << server << "\n";

#else
    // Without exceptions, check for success first
    if (!config)
    {
        std::cerr << "Parse failed:\n" << config.error() << "\n";
        return 1;
    }

    auto& tbl = config.get();
    auto server = tbl["database"]["server"].value_or(""sv);
    std::cout << "Server: " << server << "\n";
#endif

    // --- Parse from file ---
    try
    {
        auto file_config = toml::parse_file("app.toml");
        std::cout << file_config << "\n";
    }
    catch (const toml::parse_error& err)
    {
        std::cerr << "Failed to parse app.toml:\n"
                  << err.description() << "\n"
                  << "  at " << err.source() << "\n";
        return 1;
    }

    // --- Source tracking ---
    auto doc = toml::parse(R"(
        title = "Example"
        [owner]
        name = "Tom"
    )", "example.toml");

    auto* name_node = doc.get("owner");
    if (name_node)
    {
        const auto& src = name_node->source();
        std::cout << "owner defined at: "
                  << *src.path << ":"
                  << src.begin.line << ":"
                  << src.begin.column << "\n";
    }

    // --- Stream parsing ---
    std::istringstream ss(R"(key = "from stream")");
    auto stream_result = toml::parse(ss, "stream-input");
    std::cout << stream_result["key"].value_or(""sv) << "\n";

    return 0;
}
```

---

## Related Documentation

- [basic-usage.md](basic-usage.md) — Quick start guide with parsing examples
- [values.md](values.md) — Value types created by the parser
- [tables.md](tables.md) — Root table structure
- [formatting.md](formatting.md) — Serializing parsed data back to TOML
- [unicode-handling.md](unicode-handling.md) — UTF-8 handling details
