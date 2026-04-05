# toml++ — Values

## Overview

`toml::value<T>` represents leaf TOML values — the concrete data in a TOML document. Each `value` wraps one of seven native C++ types corresponding to the TOML data types.

Declared in `include/toml++/impl/value.hpp` with supporting types in `forward_declarations.hpp` and `date_time.hpp`.

---

## Native Types

The seven supported value types map to TOML types via `toml::value_type_of<T>`:

| TOML Type         | C++ Storage Type    | `node_type` Enum         | Alias              |
|-------------------|---------------------|---------------------------|--------------------|
| String            | `std::string`       | `node_type::string`       | `value<std::string>` |
| Integer           | `int64_t`           | `node_type::integer`      | `value<int64_t>`   |
| Float             | `double`            | `node_type::floating_point` | `value<double>`   |
| Boolean           | `bool`              | `node_type::boolean`      | `value<bool>`      |
| Date              | `toml::date`        | `node_type::date`         | `value<date>`      |
| Time              | `toml::time`        | `node_type::time`         | `value<time>`      |
| Date-Time         | `toml::date_time`   | `node_type::date_time`    | `value<date_time>` |

Only these seven instantiations of `value<T>` exist. The template is constrained:

```cpp
template <typename T>
class value : public node
{
    static_assert(
        impl::is_native<T>,
        "Template parameter must be one of the TOML native value types"
    );

  private:
    value_type val_;
    value_flags flags_ = value_flags::none;
};
```

Where `value_type` is the `impl::native_type_of<T>` alias.

---

## Type Traits

```cpp
// Check at compile time
toml::is_string<value<std::string>>     // true
toml::is_integer<value<int64_t>>        // true  
toml::is_floating_point<value<double>>  // true
toml::is_boolean<value<bool>>           // true
toml::is_date<value<date>>              // true
toml::is_time<value<time>>              // true
toml::is_date_time<value<date_time>>    // true

// Works on the raw types too
toml::is_integer<int64_t>              // true
toml::is_number<int64_t>               // true (integer or float)
toml::is_number<double>                // true

// Supertype checks
toml::is_value<value<int64_t>>         // true (any value<T>)
toml::is_chronological<value<date>>    // true (date, time, or date_time)
```

---

## Construction

### From Compatible Types

```cpp
toml::value<int64_t> i{ 42 };
toml::value<double> f{ 3.14 };
toml::value<std::string> s{ "hello" };
toml::value<bool> b{ true };

// Implicit promotion from smaller integer types
toml::value<int64_t> from_int{ 42 };        // int → int64_t
toml::value<int64_t> from_short{ short(5) }; // short → int64_t

// Implicit promotion from float → double
toml::value<double> from_float{ 1.5f };      // float → double
```

The `native_value_maker` mechanism handles promotions:

```cpp
// In impl namespace:
template <typename T>
struct native_value_maker;

// For integer types (int, unsigned, short, etc.):
// Promotes to int64_t

// For floating-point (float):
// Promotes to double

// For string types (const char*, string_view, etc.):
// Converts to std::string

// For char8_t strings (u8"..."):
// Transcodes to std::string
```

### Copy and Move

```cpp
toml::value<std::string> original{ "hello" };
toml::value<std::string> copy{ original };           // deep copy
toml::value<std::string> moved{ std::move(original) }; // move
```

### Assignment

```cpp
auto v = toml::value<int64_t>{ 10 };
v = 42;        // assign from raw value (operator=(ValueType))
v = copy;      // assign from another value (operator=(const value&))
v = std::move(other); // move assign
```

---

## Retrieving Values

### `get()` — Direct Access

```cpp
ValueType& get() & noexcept;
ValueType&& get() && noexcept;
const ValueType& get() const& noexcept;
```

Returns a direct reference to the stored value:

```cpp
auto v = toml::value<std::string>{ "hello" };
std::string& s = v.get();
s += " world";
std::cout << v.get() << "\n";  // "hello world"
```

### `operator ValueType&()` — Implicit Conversion

```cpp
explicit operator ValueType&() noexcept;
explicit operator const ValueType&() const noexcept;
```

```cpp
auto v = toml::value<int64_t>{ 42 };
int64_t x = static_cast<int64_t>(v);
```

### `operator*()` / `operator->()`

```cpp
ValueType& operator*() & noexcept;
const ValueType& operator*() const& noexcept;
ValueType* operator->() noexcept;
const ValueType* operator->() const noexcept;
```

Dereference-style access:

```cpp
auto v = toml::value<std::string>{ "hello" };
std::cout << v->length() << "\n";    // 5
std::cout << (*v).size() << "\n";   // 5
```

---

## Value Flags

`value_flags` is a bitmask controlling how values are formatted when serialized:

```cpp
enum class value_flags : uint16_t
{
    none = 0,
    format_as_binary          = 1,   // 0b10101
    format_as_octal           = 2,   // 0o755
    format_as_hexadecimal     = 4,   // 0xFF

    // Special sentinel (default behavior):
    // preserve_source_value_flags
};
```

### Getting / Setting Flags

```cpp
value_flags flags() const noexcept;
value<T>& flags(value_flags new_flags) noexcept;
```

```cpp
auto v = toml::value<int64_t>{ 255 };
v.flags(toml::value_flags::format_as_hexadecimal);

std::cout << toml::toml_formatter{ toml::table{ { "val", v } } };
// Output: val = 0xFF
```

### Source Format Preservation

When parsing, the library records the source format in the flags. When printing, if `preserve_source_value_flags` is used (the default), the original format is retained:

```toml
port = 0xFF
mask = 0o777
bits = 0b1010
```

After parsing and re-serializing, these retain their hex/octal/binary format.

---

## Date and Time Types

Defined in `include/toml++/impl/date_time.hpp`.

### `toml::date`

```cpp
struct date
{
    uint16_t year;
    uint8_t  month;   // 1-12
    uint8_t  day;     // 1-31

    // Comparison operators
    friend bool operator==(const date&, const date&) noexcept;
    friend bool operator!=(const date&, const date&) noexcept;
    friend bool operator< (const date&, const date&) noexcept;
    friend bool operator<=(const date&, const date&) noexcept;
    friend bool operator> (const date&, const date&) noexcept;
    friend bool operator>=(const date&, const date&) noexcept;

    // Streaming
    friend std::ostream& operator<<(std::ostream&, const date&);
};
```

```cpp
auto d = toml::date{ 2024, 1, 15 };
auto v = toml::value<toml::date>{ d };
std::cout << v << "\n";  // 2024-01-15
```

### `toml::time`

```cpp
struct time
{
    uint8_t  hour;        // 0-23
    uint8_t  minute;      // 0-59
    uint8_t  second;      // 0-59 (0-60 for leap second)
    uint32_t nanosecond;  // 0-999999999

    // Comparison and streaming operators
};
```

```cpp
auto t = toml::time{ 14, 30, 0 };
auto v = toml::value<toml::time>{ t };
std::cout << v << "\n";  // 14:30:00
```

### `toml::time_offset`

```cpp
struct time_offset
{
    int16_t minutes;   // UTC offset: -720 to +840

    // Convenience for UTC:
    static constexpr time_offset utc() noexcept { return { 0 }; }

    // Comparison operators
};
```

### `toml::date_time`

```cpp
struct date_time
{
    toml::date date;
    toml::time time;
    optional<time_offset> offset;  // nullopt = local date-time

    // Constructor overloads:
    constexpr date_time(const toml::date& d, const toml::time& t) noexcept;
    constexpr date_time(const toml::date& d, const toml::time& t,
                        const toml::time_offset& off) noexcept;

    bool is_local() const noexcept;  // true if no offset

    // Comparison and streaming operators
};
```

#### TOML Date-Time Variants

```toml
# Offset date-time (has time zone)
odt = 2024-01-15T14:30:00+05:30
odt_utc = 2024-01-15T09:00:00Z

# Local date-time (no time zone)
ldt = 2024-01-15T14:30:00

# Local date
ld = 2024-01-15

# Local time
lt = 14:30:00
```

```cpp
auto tbl = toml::parse(R"(
    odt = 2024-01-15T14:30:00+05:30
    ldt = 2024-01-15T14:30:00
    ld  = 2024-01-15
    lt  = 14:30:00
)");

auto odt = tbl["odt"].value<toml::date_time>();
// odt->offset has value, odt->is_local() == false

auto ldt = tbl["ldt"].value<toml::date_time>();
// ldt->offset is nullopt, ldt->is_local() == true

auto ld = tbl["ld"].value<toml::date>();
// ld->year == 2024, month == 1, day == 15

auto lt = tbl["lt"].value<toml::time>();
// lt->hour == 14, minute == 30, second == 0
```

---

## Type Identity

```cpp
// For value<int64_t>:
node_type type() const noexcept final;     // node_type::integer
bool is_integer() const noexcept final;     // true
bool is_number() const noexcept final;      // true
bool is_value() const noexcept final;       // true
// All other is_*() return false

value<int64_t>* as_integer() noexcept final;  // returns this
// All other as_*() return nullptr
```

---

## Retrieving Values from Nodes

From the base `node` or `node_view`, there are multiple retrieval patterns:

### `value<T>()` — Get with Type Coercion

```cpp
// As node method:
optional<T> value() const noexcept;
```

Attempts to return the value as `T`, with permitted coercions:
- `int64_t` → `int`, `unsigned`, `size_t`, etc. (bounds-checked)
- `double` → `float` (precision loss allowed)
- `double` ↔ `int64_t` (within representable range)

```cpp
auto tbl = toml::parse("x = 42");

auto as_int = tbl["x"].value<int64_t>();      // optional(42)
auto as_double = tbl["x"].value<double>();     // optional(42.0)
auto as_int32 = tbl["x"].value<int>();         // optional(42)
auto as_string = tbl["x"].value<std::string>(); // nullopt (type mismatch)
```

### `value_exact<T>()` — No Coercion

```cpp
optional<T> value_exact() const noexcept;
```

Only succeeds if the stored type exactly matches `T` (no int→double or similar coercions):

```cpp
auto tbl = toml::parse("x = 42");

auto exact_int = tbl["x"].value_exact<int64_t>();   // optional(42)
auto exact_dbl = tbl["x"].value_exact<double>();     // nullopt (it's an int)
```

### `value_or()` — With Default

```cpp
template <typename T>
auto value_or(T&& default_value) const noexcept;
```

Returns the value or a default:

```cpp
auto tbl = toml::parse("name = \"Alice\"");

auto name = tbl["name"].value_or("unknown"sv);     // "Alice"
auto age = tbl["age"].value_or(int64_t{ 0 });      // 0 (key missing)
```

---

## Comparison

```cpp
// Between values of same type
friend bool operator==(const value& lhs, const value& rhs) noexcept;

// Between value and raw type
friend bool operator==(const value<T>& lhs, const T& rhs) noexcept;
```

```cpp
auto a = toml::value<int64_t>{ 42 };
auto b = toml::value<int64_t>{ 42 };

std::cout << (a == b) << "\n";    // true
std::cout << (a == 42) << "\n";   // true
std::cout << (a != 99) << "\n";   // true
```

For `value<std::string>`, comparison also works with `std::string_view` and `const char*`:

```cpp
auto s = toml::value<std::string>{ "hello" };
std::cout << (s == "hello") << "\n";   // true
std::cout << (s == "world"sv) << "\n"; // false
```

---

## `make_value<T>`

Utility function in `make_node.hpp` for constructing values:

```cpp
template <typename T, typename... Args>
auto make_value(Args&&... args)
    -> decltype(std::make_unique<impl::wrap_node<T>>(std::forward<Args>(args)...));
```

Returns `std::unique_ptr<value<T>>`:

```cpp
auto v = toml::make_value<int64_t>(42);
// v is std::unique_ptr<toml::value<int64_t>>
```

---

## Printing

Values stream via the default formatter:

```cpp
auto v = toml::value<std::string>{ "hello world" };
std::cout << v << "\n";  // "hello world"

auto d = toml::value<toml::date>{ toml::date{ 2024, 6, 15 } };
std::cout << d << "\n";  // 2024-06-15

auto i = toml::value<int64_t>{ 255 };
i.flags(toml::value_flags::format_as_hexadecimal);
std::cout << i << "\n";  // 0xFF
```

---

## Complete Example

```cpp
#include <toml++/toml.hpp>
#include <iostream>

int main()
{
    auto config = toml::parse(R"(
        title = "My App"
        version = 3
        debug = true
        pi = 3.14159
        created = 2024-01-15T10:30:00Z
        expires = 2025-12-31
        check_time = 08:00:00
    )");

    // Type-safe retrieval with defaults
    auto title   = config["title"].value_or("Untitled"sv);
    auto version = config["version"].value_or(int64_t{ 1 });
    auto debug   = config["debug"].value_or(false);
    auto pi      = config["pi"].value_or(0.0);

    std::cout << "Title: " << title << "\n";
    std::cout << "Version: " << version << "\n";
    std::cout << "Debug: " << debug << "\n";
    std::cout << "Pi: " << pi << "\n";

    // Date-time access
    if (auto dt = config["created"].value<toml::date_time>())
    {
        std::cout << "Created: " << dt->date.year
                  << "-" << (int)dt->date.month
                  << "-" << (int)dt->date.day << "\n";

        if (!dt->is_local())
            std::cout << "  Offset: " << dt->offset->minutes << " min\n";
    }

    // Modify and re-serialize
    auto* v = config["version"].as_integer();
    if (v) *v = 4;

    std::cout << "\n" << config << "\n";

    return 0;
}
```

---

## Related Documentation

- [node-system.md](node-system.md) — Base node class and type dispatch
- [arrays.md](arrays.md) — Array container
- [tables.md](tables.md) — Table container
- [parsing.md](parsing.md) — Parsing values from TOML text
- [formatting.md](formatting.md) — Formatting values for output
