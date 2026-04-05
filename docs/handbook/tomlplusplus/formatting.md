# toml++ — Formatting

## Overview

toml++ includes three formatters for serializing a TOML node tree to text:

| Formatter | Output Format | Header |
|-----------|--------------|--------|
| `toml::toml_formatter` | Standard TOML | `toml_formatter.hpp` |
| `toml::json_formatter` | JSON | `json_formatter.hpp` |
| `toml::yaml_formatter` | YAML | `yaml_formatter.hpp` |

All three inherit from the internal `impl::formatter` base class and share common indentation and streaming infrastructure.

Formatters can be disabled entirely via `TOML_ENABLE_FORMATTERS=0`.

---

## Base Formatter (`impl::formatter`)

Declared in `include/toml++/impl/formatter.hpp`. Not directly instantiable — used through the concrete subclasses.

### `formatter_constants`

Each formatter defines a set of string constants:

```cpp
struct formatter_constants
{
    format_flags mandatory_flags;      // flags always applied
    format_flags ignored_flags;        // flags explicitly not applied

    std::string_view float_pos_inf;    // "+inf", "Infinity", ".inf"
    std::string_view float_neg_inf;    // "-inf", "-Infinity", "-.inf"
    std::string_view float_nan;        // "nan", "NaN", ".nan"

    std::string_view bool_true;        // "true"
    std::string_view bool_false;       // "false"
};
```

### `formatter_config`

```cpp
struct formatter_config
{
    format_flags flags;                // active formatting flags
};
```

### Internal State

The base class manages:
- `const node* source_` — the node being formatted
- `formatter_constants constants_` — string representations
- `formatter_config config_` — user-supplied configuration
- `int indent_` — current indentation level
- `bool naked_newline_` — tracks newline state

Helper methods:
- `increase_indent()` / `decrease_indent()` — adjust indentation
- `print_indent()` — emit current indentation
- `print_newline()` — emit newline with proper tracking
- `print_string()` — format a TOML string with proper escaping
- `print_value()` — format a leaf value (delegates to constants for inf/nan/bool)

---

## `format_flags`

Bitmask enum controlling formatting behavior:

```cpp
enum class format_flags : uint64_t
{
    none                          = 0,
    quote_dates_and_times         = (1ull << 0),
    quote_infinities_and_nans     = (1ull << 1),
    allow_literal_strings         = (1ull << 2),
    allow_multi_line_strings      = (1ull << 3),
    allow_real_tabs_in_values     = (1ull << 4),
    allow_unicode_strings         = (1ull << 5),
    allow_binary_integers         = (1ull << 6),
    allow_octal_integers          = (1ull << 7),
    allow_hexadecimal_integers    = (1ull << 8),
    indent_sub_tables             = (1ull << 9),
    indent_array_elements         = (1ull << 10),
    indentation                   = indent_sub_tables | indent_array_elements,
    relaxed_float_precision       = (1ull << 11),
    terse_key_value_pairs         = (1ull << 12),
};
```

### Flag Details

| Flag | Effect |
|------|--------|
| `quote_dates_and_times` | Emit dates/times as `"2024-01-15"` instead of `2024-01-15` |
| `quote_infinities_and_nans` | Emit `"inf"` / `"nan"` as quoted strings |
| `allow_literal_strings` | Use `'single quotes'` where possible |
| `allow_multi_line_strings` | Use `"""multi-line"""` where appropriate |
| `allow_real_tabs_in_values` | Emit `\t` as literal tab instead of escape |
| `allow_unicode_strings` | Keep Unicode characters instead of escaping to `\uXXXX` |
| `allow_binary_integers` | Emit `0b1010` for binary-flagged integers |
| `allow_octal_integers` | Emit `0o755` for octal-flagged integers |
| `allow_hexadecimal_integers` | Emit `0xFF` for hex-flagged integers |
| `indent_sub_tables` | Indent sub-table content |
| `indent_array_elements` | Indent array elements on separate lines |
| `relaxed_float_precision` | Use less precision for floats |
| `terse_key_value_pairs` | Emit `key=value` instead of `key = value` |

---

## TOML Formatter

### Constants

```cpp
static constexpr formatter_constants toml_formatter_constants = {
    // mandatory_flags:
    format_flags::allow_literal_strings
        | format_flags::allow_multi_line_strings
        | format_flags::allow_unicode_strings
        | format_flags::allow_binary_integers
        | format_flags::allow_octal_integers
        | format_flags::allow_hexadecimal_integers,

    // ignored_flags:
    format_flags::quote_dates_and_times
        | format_flags::quote_infinities_and_nans,

    // float_pos_inf, float_neg_inf, float_nan:
    "inf"sv, "-inf"sv, "nan"sv,

    // bool_true, bool_false:
    "true"sv, "false"sv
};
```

### Default Flags

```cpp
static constexpr format_flags default_flags =
    format_flags::allow_literal_strings
    | format_flags::allow_multi_line_strings
    | format_flags::allow_unicode_strings
    | format_flags::allow_binary_integers
    | format_flags::allow_octal_integers
    | format_flags::allow_hexadecimal_integers
    | format_flags::indentation;
```

### Construction

```cpp
// Format a table (most common)
toml::toml_formatter fmt{ my_table };
std::cout << fmt;

// With custom flags
toml::toml_formatter fmt2{ my_table, format_flags::indent_sub_tables };
std::cout << fmt2;

// Format any node (array, value, etc.)
toml::toml_formatter fmt3{ my_array };
std::cout << fmt3;
```

### Key Path Tracking

The TOML formatter maintains a `key_path_` to correctly generate fully-qualified section headers:

```cpp
auto tbl = toml::parse(R"(
    [server.database]
    host = "localhost"
    port = 5432
)");

std::cout << toml::toml_formatter{ tbl };
```

Output:
```toml
[server.database]
host = "localhost"
port = 5432
```

### Inline Tables

Tables marked as inline are output as `{ key = val, ... }`:

```cpp
auto tbl = toml::table{
    { "point", toml::table{ { "x", 1 }, { "y", 2 } } }
};
tbl["point"].as_table()->is_inline(true);

std::cout << toml::toml_formatter{ tbl };
// Output: point = { x = 1, y = 2 }
```

### Streaming

The default `operator<<` for nodes uses `toml_formatter`:

```cpp
auto tbl = toml::parse("key = 42");
std::cout << tbl << "\n";
// Equivalent to: std::cout << toml::toml_formatter{ tbl } << "\n";
```

---

## JSON Formatter

### Constants

```cpp
static constexpr formatter_constants json_formatter_constants = {
    // mandatory_flags:
    format_flags::quote_dates_and_times
        | format_flags::quote_infinities_and_nans,

    // ignored_flags:
    format_flags::allow_literal_strings
        | format_flags::allow_multi_line_strings
        | format_flags::allow_binary_integers
        | format_flags::allow_octal_integers
        | format_flags::allow_hexadecimal_integers,

    // float_pos_inf, float_neg_inf, float_nan:
    "Infinity"sv, "-Infinity"sv, "NaN"sv,

    // bool_true, bool_false:
    "true"sv, "false"sv
};
```

### Key Differences from TOML Formatter

- **Dates and times** are always quoted: `"2024-01-15"` instead of `2024-01-15`
- **Infinity and NaN** are quoted: `"Infinity"`, `"-Infinity"`, `"NaN"`
- **Integers** always in decimal (no `0xFF`, `0o777`, `0b1010`)
- **Object keys** always quoted
- **No section headers** — uses nested `{ }` structure
- **Commas** separate elements

### Default Flags

```cpp
static constexpr format_flags default_flags =
    format_flags::quote_dates_and_times
    | format_flags::quote_infinities_and_nans
    | format_flags::indentation;
```

### Usage

```cpp
auto tbl = toml::parse(R"(
    [server]
    host = "localhost"
    port = 8080
    tags = ["web", "api"]
)");

std::cout << toml::json_formatter{ tbl } << "\n";
```

Output:
```json
{
    "server": {
        "host": "localhost",
        "port": 8080,
        "tags": [
            "web",
            "api"
        ]
    }
}
```

### Transcoding Example

From the `examples/toml_to_json_transcoder.cpp`:

```cpp
#include <toml++/toml.hpp>
#include <iostream>

int main(int argc, char** argv)
{
    toml::table tbl;
    try
    {
        tbl = toml::parse(std::cin, "stdin"sv);
    }
    catch (const toml::parse_error& err)
    {
        std::cerr << err << "\n";
        return 1;
    }

    std::cout << toml::json_formatter{ tbl } << "\n";
    return 0;
}
```

---

## YAML Formatter

### Constants

```cpp
static constexpr formatter_constants yaml_formatter_constants = {
    // mandatory_flags:
    format_flags::quote_dates_and_times,

    // ignored_flags:
    format_flags::allow_literal_strings
        | format_flags::allow_multi_line_strings
        | format_flags::allow_binary_integers
        | format_flags::allow_octal_integers
        | format_flags::allow_hexadecimal_integers,

    // float_pos_inf, float_neg_inf, float_nan:
    ".inf"sv, "-.inf"sv, ".nan"sv,

    // bool_true, bool_false:
    "true"sv, "false"sv
};
```

### Key Differences

- **Indentation** uses 2 spaces (indent level 1 = 2 spaces)
- **No braces or brackets** — uses YAML's indentation-based structure
- **Dates quoted**: `"2024-01-15"`
- **Inf/NaN**: `.inf`, `-.inf`, `.nan` (YAML style)
- **Array elements** prefixed with `- `
- **No commas**

### Default Flags

```cpp
static constexpr format_flags default_flags =
    format_flags::quote_dates_and_times
    | format_flags::allow_unicode_strings
    | format_flags::indentation;
```

### Usage

```cpp
auto tbl = toml::parse(R"(
    [server]
    host = "localhost"
    port = 8080
    tags = ["web", "api"]
)");

std::cout << toml::yaml_formatter{ tbl } << "\n";
```

Output:
```yaml
server:
  host: "localhost"
  port: 8080
  tags:
    - "web"
    - "api"
```

---

## Printing Individual Nodes

Any node can be formatted, not just tables:

```cpp
auto arr = toml::array{ 1, 2, 3, "four" };
std::cout << toml::toml_formatter{ arr } << "\n";
// [1, 2, 3, "four"]

std::cout << toml::json_formatter{ arr } << "\n";
// [1, 2, 3, "four"]

auto val = toml::value<std::string>{ "hello" };
std::cout << toml::toml_formatter{ val } << "\n";
// "hello"
```

---

## Writing to Files

```cpp
#include <fstream>

auto tbl = toml::parse_file("input.toml");

// Write as TOML
{
    std::ofstream out("output.toml");
    out << toml::toml_formatter{ tbl };
}

// Write as JSON
{
    std::ofstream out("output.json");
    out << toml::json_formatter{ tbl };
}

// Write as YAML
{
    std::ofstream out("output.yaml");
    out << toml::yaml_formatter{ tbl };
}
```

---

## Customizing Output

### Disabling Indentation

```cpp
auto fmt = toml::toml_formatter{ tbl, format_flags::none };
std::cout << fmt;
```

### Terse Key-Value Pairs

```cpp
auto fmt = toml::toml_formatter{
    tbl,
    format_flags::terse_key_value_pairs | format_flags::indentation
};
// Output: key=value instead of key = value
```

### Preserving Source Format

By default, integer format flags from parsing are preserved. A value parsed from `0xFF` will serialize back as `0xFF`:

```cpp
auto tbl = toml::parse("mask = 0xFF");
std::cout << tbl << "\n";
// Output: mask = 0xFF
```

This works because the parser sets `value_flags::format_as_hexadecimal` on the value, and the TOML formatter has `allow_hexadecimal_integers` in its mandatory flags.

---

## Formatter Comparison

| Feature | `toml_formatter` | `json_formatter` | `yaml_formatter` |
|---------|-----------------|-----------------|-----------------|
| Format | TOML v1.0 | JSON | YAML |
| Indentation | Tab (default) | 4 spaces | 2 spaces |
| Infinity | `inf` | `"Infinity"` | `.inf` |
| NaN | `nan` | `"NaN"` | `.nan` |
| Dates | Unquoted | Quoted | Quoted |
| Integer formats | Hex/Oct/Bin | Decimal only | Decimal only |
| Literal strings | Yes | No | No |
| Multi-line strings | Yes | No | No |
| Section headers | `[table]` | N/A | N/A |
| Inline tables | `{ k = v }` | N/A | N/A |

---

## Complete Example

```cpp
#include <toml++/toml.hpp>
#include <iostream>
#include <sstream>

int main()
{
    auto config = toml::parse(R"(
        title = "My Config"
        debug = true
        max_connections = 0xFF

        [server]
        host = "localhost"
        port = 8080
        started = 2024-01-15T10:30:00Z

        [server.ssl]
        enabled = true
        cert = "/etc/ssl/cert.pem"

        [[server.routes]]
        path = "/"
        handler = "index"

        [[server.routes]]
        path = "/api"
        handler = "api"
    )");

    // TOML output (the default)
    std::cout << "=== TOML ===\n";
    std::cout << config << "\n\n";

    // JSON output
    std::cout << "=== JSON ===\n";
    std::cout << toml::json_formatter{ config } << "\n\n";

    // YAML output
    std::cout << "=== YAML ===\n";
    std::cout << toml::yaml_formatter{ config } << "\n\n";

    // Terse TOML
    std::cout << "=== Terse TOML ===\n";
    std::cout << toml::toml_formatter{
        config,
        toml::format_flags::terse_key_value_pairs
            | toml::format_flags::indentation
    } << "\n";

    // Format to string
    std::ostringstream ss;
    ss << toml::json_formatter{ config };
    std::string json_string = ss.str();

    return 0;
}
```

---

## Related Documentation

- [parsing.md](parsing.md) — Parsing TOML into node trees
- [values.md](values.md) — Value flags affecting output format
- [tables.md](tables.md) — Inline table formatting
- [basic-usage.md](basic-usage.md) — Quick formatting examples
