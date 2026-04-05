# toml++ — Architecture

## Overview

toml++ implements a tree-based data model for TOML documents. A parsed TOML document becomes a tree of `toml::node` objects, with `toml::table` as the root. The architecture centers on:

1. A polymorphic node hierarchy (`node` → `table`, `array`, `value<T>`)
2. A recursive-descent parser that builds trees
3. Formatter classes that serialize trees back to text
4. A path system for structured navigation

All public types live in the `toml` namespace. Internal implementation details live in `toml::impl` (an ABI-namespaced detail namespace).

---

## Class Hierarchy

```
toml::node (abstract base)
├── toml::table          — ordered map of key → node*
├── toml::array          — vector of node*
└── toml::value<T>       — leaf node holding a value
    ├── value<std::string>
    ├── value<int64_t>
    ├── value<double>
    ├── value<bool>
    ├── value<toml::date>
    ├── value<toml::time>
    └── value<toml::date_time>
```

Supporting types:
```
toml::node_view<T>       — non-owning optional reference to a node
toml::key                — string + source_region metadata
toml::path               — vector of path_component
toml::path_component     — key string or array index
toml::source_position    — line + column
toml::source_region      — begin + end positions + path
toml::parse_error        — error description + source_region
toml::parse_result       — table | parse_error (no-exceptions mode)
```

Formatter hierarchy:
```
impl::formatter (base, protected)
├── toml::toml_formatter    — TOML output
├── toml::json_formatter    — JSON output
└── toml::yaml_formatter    — YAML output
```

---

## `toml::node` — The Abstract Base Class

Defined in `include/toml++/impl/node.hpp`, `toml::node` is the polymorphic base of all TOML tree nodes. It is declared as `TOML_ABSTRACT_INTERFACE`, meaning it has pure virtual methods and cannot be instantiated directly.

### Private Members

```cpp
class node
{
  private:
    source_region source_{};

    template <typename T>
    decltype(auto) get_value_exact() const noexcept(...);

    // ref_type_ and ref_type — template aliases for ref() return types
    // do_ref() — static helper for ref() implementation
```

The `source_` member records where this node was defined in the original TOML document (line, column, file path).

### Protected Members

```cpp
  protected:
    node() noexcept;
    node(const node&) noexcept;
    node(node&&) noexcept;
    node& operator=(const node&) noexcept;
    node& operator=(node&&) noexcept;

    // ref_cast<T>() — unsafe downcast helpers (all four ref-qualifications)
    template <typename T> ref_cast_type<T, node&> ref_cast() & noexcept;
    template <typename T> ref_cast_type<T, node&&> ref_cast() && noexcept;
    template <typename T> ref_cast_type<T, const node&> ref_cast() const& noexcept;
    template <typename T> ref_cast_type<T, const node&&> ref_cast() const&& noexcept;
```

Constructors and assignment operators are `protected` to prevent direct instantiation. `ref_cast<T>()` performs `reinterpret_cast`-based downcasts, used internally by `ref<T>()`.

### Public Interface — Type Checks

Every `node` provides a complete set of virtual type-checking methods:

```cpp
  public:
    virtual ~node() noexcept;

    // Homogeneity checks
    virtual bool is_homogeneous(node_type ntype, node*& first_nonmatch) noexcept = 0;
    virtual bool is_homogeneous(node_type ntype, const node*& first_nonmatch) const noexcept = 0;
    virtual bool is_homogeneous(node_type ntype) const noexcept = 0;
    template <typename ElemType = void>
    bool is_homogeneous() const noexcept;

    // Type identity
    virtual node_type type() const noexcept = 0;
    virtual bool is_table() const noexcept = 0;
    virtual bool is_array() const noexcept = 0;
    virtual bool is_array_of_tables() const noexcept;
    virtual bool is_value() const noexcept = 0;
    virtual bool is_string() const noexcept = 0;
    virtual bool is_integer() const noexcept = 0;
    virtual bool is_floating_point() const noexcept = 0;
    virtual bool is_number() const noexcept = 0;
    virtual bool is_boolean() const noexcept = 0;
    virtual bool is_date() const noexcept = 0;
    virtual bool is_time() const noexcept = 0;
    virtual bool is_date_time() const noexcept = 0;

    // Template type check
    template <typename T>
    bool is() const noexcept;
```

The `is<T>()` template dispatches to the appropriate virtual method using `if constexpr`:

```cpp
template <typename T>
bool is() const noexcept
{
    using type = impl::remove_cvref<impl::unwrap_node<T>>;
    if constexpr (std::is_same_v<type, table>)
        return is_table();
    else if constexpr (std::is_same_v<type, array>)
        return is_array();
    else if constexpr (std::is_same_v<type, std::string>)
        return is_string();
    // ... etc for int64_t, double, bool, date, time, date_time
}
```

### Public Interface — Type Casts

```cpp
    // Downcasts — return nullptr if type doesn't match
    virtual table* as_table() noexcept = 0;
    virtual array* as_array() noexcept = 0;
    virtual toml::value<std::string>* as_string() noexcept = 0;
    virtual toml::value<int64_t>* as_integer() noexcept = 0;
    virtual toml::value<double>* as_floating_point() noexcept = 0;
    virtual toml::value<bool>* as_boolean() noexcept = 0;
    virtual toml::value<date>* as_date() noexcept = 0;
    virtual toml::value<time>* as_time() noexcept = 0;
    virtual toml::value<date_time>* as_date_time() noexcept = 0;
    // + const overloads for all of the above

    // Template downcast
    template <typename T>
    impl::wrap_node<T>* as() noexcept;
    template <typename T>
    const impl::wrap_node<T>* as() const noexcept;
```

`as<T>()` is the unified template that dispatches to `as_table()`, `as_string()`, etc.

### Public Interface — Value Retrieval

```cpp
    // Exact-match value retrieval
    template <typename T>
    optional<T> value_exact() const noexcept(...);

    // Permissive value retrieval (allows conversions)
    template <typename T>
    optional<T> value() const noexcept(...);

    // Value with default
    template <typename T>
    auto value_or(T&& default_value) const noexcept(...);
```

`value_exact<T>()` only succeeds if the node contains exactly type `T`. `value<T>()` is more lenient, allowing integer-to-float conversions and the like. `value_or()` returns the value if present, otherwise the given default.

### Public Interface — Reference Access

```cpp
    template <typename T>
    decltype(auto) ref() & noexcept;
    template <typename T>
    decltype(auto) ref() && noexcept;
    template <typename T>
    decltype(auto) ref() const& noexcept;
    template <typename T>
    decltype(auto) ref() const&& noexcept;
```

`ref<T>()` provides direct reference access to the underlying value. It asserts the type matches and is UB if it doesn't.

### Public Interface — Visitation

```cpp
    template <typename Func>
    decltype(auto) visit(Func&& visitor) & noexcept(...);
    // + &&, const&, const&& overloads
```

Calls the visitor with the concrete node type. The visitor receives the actual `table&`, `array&`, or `value<T>&`.

### Source Region

```cpp
    const source_region& source() const noexcept;
```

Returns where this node was defined in the source document.

---

## `toml::table` — TOML Tables

Declared in `include/toml++/impl/table.hpp`, `toml::table` extends `node` and models an ordered map of keys to nodes.

### Internal Storage

```cpp
class table : public node
{
  private:
    using map_type = std::map<toml::key, impl::node_ptr, std::less<>>;
    map_type map_;
    bool inline_ = false;
```

- The backing container is `std::map<toml::key, std::unique_ptr<node>, std::less<>>`.
- `std::less<>` enables heterogeneous lookup (search by `std::string_view` without constructing a `key`).
- `inline_` tracks whether the table should be serialized as an inline table `{ ... }`.
- Insertion order is maintained because `toml::key` provides comparison operators that match `std::map`'s ordered semantics.

### Iterators

The table uses custom `impl::table_iterator<IsConst>` which wraps the map iterator and produces `table_proxy_pair<IsConst>` references:

```cpp
template <bool IsConst>
struct table_proxy_pair
{
    using value_type = std::conditional_t<IsConst, const node, node>;
    const toml::key& first;
    value_type& second;
};
```

This means iterating a table yields `(const key&, node&)` pairs, not `(const key&, unique_ptr<node>&)`. The `unique_ptr` layer is hidden.

Public type aliases:
```cpp
using table_iterator = impl::table_iterator<false>;
using const_table_iterator = impl::table_iterator<true>;
```

### Construction

```cpp
table() noexcept;                                      // default
table(const table&);                                   // deep copy
table(table&& other) noexcept;                         // move
explicit table(std::initializer_list<impl::table_init_pair> kvps);
```

The initializer-list constructor accepts `table_init_pair` objects, each containing a key and a value:

```cpp
struct table_init_pair
{
    mutable toml::key key;
    mutable node_ptr value;  // std::unique_ptr<node>

    template <typename K, typename V>
    table_init_pair(K&& k, V&& v, value_flags flags = preserve_source_value_flags);
};
```

This enables the idiomatic construction:
```cpp
auto tbl = toml::table{
    { "name", "toml++" },
    { "version", 3 },
    { "nested", toml::table{ { "key", true } } }
};
```

### Key Operations

| Method | Description |
|--------|-------------|
| `size()` | Number of key-value pairs |
| `empty()` | Whether the table is empty |
| `get(key)` | Get node pointer by key, or nullptr |
| `get_as<T>(key)` | Get typed node pointer, or nullptr |
| `contains(key)` | Check if key exists |
| `operator[](key)` | Returns `node_view` (safe, never null-deref) |
| `at(key)` | Returns node reference, throws if missing |
| `insert(key, val)` | Insert if not present |
| `insert_or_assign(key, val)` | Insert or replace |
| `emplace<T>(key, args...)` | Construct in place if not present |
| `erase(key)` | Remove by key |
| `erase(iterator)` | Remove by iterator |
| `clear()` | Remove all entries |

### Metadata

```cpp
bool is_inline() const noexcept;
void is_inline(bool val) noexcept;
```

Controls inline table formatting: `{ a = 1, b = 2 }` vs. multi-line.

---

## `toml::array` — TOML Arrays

Declared in `include/toml++/impl/array.hpp`, `toml::array` extends `node` and models a heterogeneous sequence.

### Internal Storage

```cpp
class array : public node
{
  private:
    std::vector<impl::node_ptr> elems_;
```

Each element is a `std::unique_ptr<node>`. The array can contain any mix of value types, tables, and nested arrays.

### Iterators

`impl::array_iterator<IsConst>` wraps the vector iterator and dereferences the `unique_ptr`, yielding `node&` references:

```cpp
using array_iterator = impl::array_iterator<false>;
using const_array_iterator = impl::array_iterator<true>;
```

It satisfies `RandomAccessIterator` requirements (unlike `table_iterator` which is `BidirectionalIterator`).

### Key Operations

| Method | Description |
|--------|-------------|
| `size()` | Number of elements |
| `empty()` | Whether the array is empty |
| `capacity()` | Reserved capacity |
| `reserve(n)` | Reserve capacity |
| `shrink_to_fit()` | Release excess capacity |
| `operator[](index)` | Returns `node&` (no bounds check) |
| `at(index)` | Returns `node&` (bounds-checked, throws) |
| `front()` / `back()` | First / last element |
| `get(index)` | Returns `node*` or nullptr |
| `get_as<T>(index)` | Returns typed pointer or nullptr |
| `push_back(val)` | Append element |
| `emplace_back<T>(args...)` | Construct at end |
| `insert(pos, val)` | Insert at position |
| `emplace(pos, args...)` | Construct at position |
| `erase(pos)` | Remove at position |
| `erase(first, last)` | Remove range |
| `pop_back()` | Remove last |
| `clear()` | Remove all |
| `resize(n)` | Resize (default-constructed elements) |
| `truncate(n)` | Remove elements beyond index n |
| `flatten()` | Flatten nested arrays |
| `prune()` | Remove empty tables and arrays recursively |

### Homogeneity

```cpp
bool is_homogeneous(node_type ntype) const noexcept;
bool is_homogeneous(node_type ntype, node*& first_nonmatch) noexcept;
template <typename ElemType = void>
bool is_homogeneous() const noexcept;
```

Returns `true` if all elements are the same type. Returns `false` for empty arrays.

### for_each

```cpp
template <typename Func>
array& for_each(Func&& visitor) & noexcept(...);
```

Iterates elements, calling the visitor with each concrete element type. Supports early exit by returning `bool` from the visitor (on compilers without the GCC 7 bug).

---

## `toml::value<T>` — Leaf Values

Declared in `include/toml++/impl/value.hpp`, `toml::value<T>` is a class template holding a single TOML value.

### Template Parameter Constraints

`T` must be one of the native TOML value types:
- `std::string`
- `int64_t`
- `double`
- `bool`
- `toml::date`
- `toml::time`
- `toml::date_time`

```cpp
template <typename ValueType>
class value : public node
{
    static_assert(impl::is_native<ValueType> && !impl::is_cvref<ValueType>);

  private:
    ValueType val_;
    value_flags flags_ = value_flags::none;
```

### Type Aliases

```cpp
using value_type = ValueType;
using value_arg = /* conditional type */;
```

`value_arg` differs by value type:
- `int64_t`, `double`, `bool` → passed by value
- `std::string` → passed as `std::string_view`
- `date`, `time`, `date_time` → passed as `const value_type&`

### Key Operations

```cpp
// Access the underlying value
ValueType& get() & noexcept;
ValueType&& get() && noexcept;
const ValueType& get() const& noexcept;
const ValueType&& get() const&& noexcept;

// Implicit conversion operator
operator ValueType&() noexcept;
operator const ValueType&() const noexcept;

// Value flags (integer format: binary, octal, hex)
value_flags flags() const noexcept;
value<ValueType>& flags(value_flags new_flags) noexcept;
```

### Construction

`value<T>` supports variadic construction via `impl::native_value_maker`:

```cpp
template <typename... Args>
explicit value(Args&&... args);

value(const value& other) noexcept;
value(const value& other, value_flags flags) noexcept;
value(value&& other) noexcept;
value(value&& other, value_flags flags) noexcept;
```

Special handling exists for:
- `char8_t` strings (C++20): converted via `reinterpret_cast`
- Wide strings (Windows): narrowed via `impl::narrow()`

---

## Date/Time Types

Defined in `include/toml++/impl/date_time.hpp`:

### `toml::date`

```cpp
struct date
{
    uint16_t year;
    uint8_t month;   // 1-12
    uint8_t day;     // 1-31

    constexpr date(Y y, M m, D d) noexcept;
    // Comparison operators: ==, !=, <, <=, >, >=
    // Stream output: YYYY-MM-DD
};
```

### `toml::time`

```cpp
struct time
{
    uint8_t hour;        // 0-23
    uint8_t minute;      // 0-59
    uint8_t second;      // 0-59
    uint32_t nanosecond; // 0-999999999

    constexpr time(H h, M m, S s = 0, NS ns = 0) noexcept;
    // Comparison operators, stream output: HH:MM:SS.nnnnnnnnn
};
```

### `toml::time_offset`

```cpp
struct time_offset
{
    int16_t minutes;  // -1440 to +1440

    constexpr time_offset(H h, M m) noexcept;
    // Comparison operators, stream output: +HH:MM or -HH:MM
};
```

### `toml::date_time`

```cpp
struct date_time
{
    toml::date date;
    toml::time time;
    optional<toml::time_offset> offset;

    // If offset is present, it's an offset date-time
    // If offset is absent, it's a local date-time
    bool is_local() const noexcept;
};
```

---

## `toml::key` — Table Keys

Defined in `include/toml++/impl/key.hpp`:

```cpp
class key
{
  private:
    std::string key_;
    source_region source_;

  public:
    explicit key(std::string_view k, source_region&& src = {});
    explicit key(std::string&& k, source_region&& src = {}) noexcept;
    explicit key(const char* k, source_region&& src = {});

    // String access
    std::string_view str() const noexcept;
    operator std::string_view() const noexcept;
    bool empty() const noexcept;

    // Source tracking
    const source_region& source() const noexcept;

    // Comparison — compares the string content, not source position
    friend bool operator==(const key& lhs, const key& rhs) noexcept;
    friend bool operator<(const key& lhs, const key& rhs) noexcept;
    // + heterogeneous comparisons with std::string_view
};
```

Keys carry source position metadata from parsing, but comparisons only consider the string content.

---

## Parser Design

The parser is a recursive-descent UTF-8 parser implemented in `impl::parser` (defined in `parser.inl`). It operates on a stream of UTF-8 codepoints.

### Parse Entry Points

```cpp
// Defined in parser.hpp
parse_result parse(std::string_view doc, std::string_view source_path = {});
parse_result parse(std::string_view doc, std::string&& source_path);
parse_result parse_file(std::string_view file_path);

// Stream overloads
parse_result parse(std::istream& doc, std::string_view source_path = {});
```

### Return Type: `parse_result`

When exceptions are enabled (`TOML_EXCEPTIONS=1`):
```cpp
using parse_result = table;  // parse() throws on error
```

When exceptions are disabled (`TOML_EXCEPTIONS=0`):
```cpp
class parse_result  // discriminated union: table or parse_error
{
    bool err_;
    union { toml::table; parse_error; } storage_;

  public:
    explicit operator bool() const noexcept;  // true = success
    table& table() &;
    parse_error& error() &;
    // + iterator accessors for safe ranged-for on failure
};
```

### Error Type

```cpp
class parse_error /* : public std::runtime_error (with exceptions) */
{
    std::string_view description() const noexcept;
    const source_region& source() const noexcept;
};
```

### Parser Internals

The `impl::parser` class stores:
- A UTF-8 byte stream reader
- A source position tracker
- The root table being built
- The current implicit table stack (for dotted keys and `[section.headers]`)

Parsing proceeds top-down:
1. Skip BOM if present
2. Parse key-value pairs, table headers (`[table]`), and array-of-tables headers (`[[array]]`)
3. For each key-value pair, parse the key (bare or quoted), then the value
4. Values are parsed based on the leading character(s): strings, numbers, booleans, dates/times, arrays, inline tables

Node allocation uses `impl::make_node()` which dispatches through `impl::make_node_impl_specialized()`:
```cpp
template <typename T>
auto* make_node_impl_specialized(T&& val, value_flags flags)
{
    using unwrapped_type = unwrap_node<remove_cvref<T>>;
    if constexpr (is_one_of<unwrapped_type, array, table>)
        return new unwrapped_type(static_cast<T&&>(val));
    else
    {
        using native_type = native_type_of<unwrapped_type>;
        using value_type = value<native_type>;
        return new value_type{ static_cast<T&&>(val) };
    }
}
```

---

## Formatter Design

### Base Class: `impl::formatter`

Defined in `include/toml++/impl/formatter.hpp`:

```cpp
class formatter
{
  private:
    const node* source_;
    const parse_result* result_;  // for no-exceptions mode
    const formatter_constants* constants_;
    formatter_config config_;
    size_t indent_columns_;
    format_flags int_format_mask_;
    std::ostream* stream_;
    int indent_;
    bool naked_newline_;

  protected:
    // Stream management
    void attach(std::ostream& stream) noexcept;
    void detach() noexcept;

    // Output primitives
    void print_newline(bool force = false);
    void print_indent();
    void print_unformatted(char);
    void print_unformatted(std::string_view);

    // Typed printing
    void print_string(std::string_view str, bool allow_multi_line, bool allow_bare, bool allow_literal_whitespace);
    void print(const value<std::string>&);
    void print(const value<int64_t>&);
    void print(const value<double>&);
    void print(const value<bool>&);
    void print(const value<date>&);
    void print(const value<time>&);
    void print(const value<date_time>&);
    void print_value(const node&, node_type);

    // Configuration queries
    bool indent_array_elements() const noexcept;
    bool indent_sub_tables() const noexcept;
    bool literal_strings_allowed() const noexcept;
    bool multi_line_strings_allowed() const noexcept;
    bool unicode_strings_allowed() const noexcept;
    bool terse_kvps() const noexcept;
    bool force_multiline_arrays() const noexcept;
};
```

### Formatter Constants

Each concrete formatter defines its behavior via `formatter_constants`:

```cpp
struct formatter_constants
{
    format_flags mandatory_flags;   // always-on flags
    format_flags ignored_flags;     // always-off flags
    std::string_view float_pos_inf; // e.g., "inf", "Infinity", ".inf"
    std::string_view float_neg_inf;
    std::string_view float_nan;
    std::string_view bool_true;
    std::string_view bool_false;
};
```

| Formatter | pos_inf | neg_inf | nan | bool_true | bool_false |
|-----------|---------|---------|-----|-----------|------------|
| `toml_formatter` | `"inf"` | `"-inf"` | `"nan"` | `"true"` | `"false"` |
| `json_formatter` | `"Infinity"` | `"-Infinity"` | `"NaN"` | `"true"` | `"false"` |
| `yaml_formatter` | `".inf"` | `"-.inf"` | `".NAN"` | `"true"` | `"false"` |

### `toml::toml_formatter`

Inherits `impl::formatter`. Serializes to valid TOML format.

Key behaviors:
- Tracks a `key_path_` vector for producing `[table.paths]`
- Manages `pending_table_separator_` for blank lines between sections
- Uses 4-space indentation by default (`"    "sv`)
- Respects `is_inline()` on tables
- Default flags include all `allow_*` flags and `indentation`

### `toml::json_formatter`

Outputs valid JSON. Key differences from TOML formatter:
- Mandatory `quote_dates_and_times` (dates become quoted strings)
- Ignores `allow_literal_strings` and `allow_multi_line_strings`
- Default includes `quote_infinities_and_nans`
- Uses 4-space indentation

### `toml::yaml_formatter`

Outputs YAML. Key differences:
- Uses 2-space indentation (`"  "sv`)
- Mandatory `quote_dates_and_times` and `indentation`
- Ignores `allow_multi_line_strings`
- Custom string printing via `print_yaml_string()`
- YAML-style inf/nan representation (`.inf`, `-.inf`, `.NAN`)

### Streaming Pattern

All formatters use the same attach/print/detach pattern:

```cpp
friend std::ostream& operator<<(std::ostream& lhs, toml_formatter& rhs)
{
    rhs.attach(lhs);
    rhs.key_path_.clear();  // (toml_formatter specific)
    rhs.print();
    rhs.detach();
    return lhs;
}
```

---

## `toml::node_view<T>` — Safe Node References

Defined in `include/toml++/impl/node_view.hpp`:

```cpp
template <typename ViewedType>
class node_view
{
    static_assert(impl::is_one_of<ViewedType, toml::node, const toml::node>);

  private:
    mutable viewed_type* node_ = nullptr;

  public:
    using viewed_type = ViewedType;

    node_view() noexcept = default;
    explicit node_view(viewed_type* node) noexcept;
    explicit node_view(viewed_type& node) noexcept;

    explicit operator bool() const noexcept;
    viewed_type* node() const noexcept;
```

`node_view` wraps a pointer to a `node` (or `const node`) and provides the same interface as `node` — type checks, casts, value retrieval — but safely handles null (returns empty optionals, false booleans, empty views on subscript).

The key design feature is chainable subscript operators:

```cpp
node_view operator[](std::string_view key) const noexcept;
node_view operator[](size_t index) const noexcept;
node_view operator[](const path& p) const noexcept;
```

These return empty views when the key/index doesn't exist, so you can chain deeply without null checks:

```cpp
auto val = tbl["section"]["subsection"]["key"].value_or(42);
// Safe even if any intermediate node is missing
```

---

## Source Tracking

### `toml::source_position`

```cpp
struct source_position
{
    source_index line;    // 1-based
    source_index column;  // 1-based

    explicit constexpr operator bool() const noexcept;
    // Comparison operators
};
```

### `toml::source_region`

```cpp
struct source_region
{
    source_position begin;
    source_position end;
    source_path_ptr path;  // std::shared_ptr<const std::string>
};
```

Every node carries a `source_region` accessible via `node::source()`. For programmatically-constructed nodes, the source region is default (zeroed). For parsed nodes, it tracks the exact file location.

---

## Ownership Model

The ownership model is straightforward:
- `toml::table` owns its child nodes via `std::map<key, std::unique_ptr<node>>`
- `toml::array` owns its child nodes via `std::vector<std::unique_ptr<node>>`
- `toml::value<T>` owns its stored value directly (by value)
- `toml::node_view<T>` is non-owning (raw pointer)
- `toml::parse_result` owns either a `table` or a `parse_error` (union storage)

Copying a `table` or `array` performs a deep copy of the entire subtree. Moving transfers ownership with no allocation.

---

## Thread Safety

toml++ does not provide internal synchronization. A parsed `table` tree can be read concurrently from multiple threads, but modification requires external synchronization. The parser itself is not re-entrant — each call to `parse()` creates a new internal parser instance.

---

## Memory Allocation

All heap allocation uses the global `operator new` (no custom allocators). Nodes are individually heap-allocated even in arrays. The `make_node.hpp` factory always calls `new`:

```cpp
return new unwrapped_type(static_cast<T&&>(val));
// or
return new value_type{ static_cast<T&&>(val) };
```

This makes the library simple but means that large TOML documents with many small values will create many small allocations.

---

## ABI Namespaces

toml++ uses conditional inline namespaces to prevent ODR violations when mixing translation units compiled with different configuration macros:

```cpp
TOML_ABI_NAMESPACE_BOOL(TOML_EXCEPTIONS, ex, noex);
// Creates either:
//   namespace ex { ... }    // when TOML_EXCEPTIONS == 1
//   namespace noex { ... }  // when TOML_EXCEPTIONS == 0

TOML_ABI_NAMESPACE_BOOL(TOML_HAS_CUSTOM_OPTIONAL_TYPE, custopt, stdopt);
```

This means `toml::ex::parse_result` (exceptions enabled) and `toml::noex::parse_result` (exceptions disabled) are different types, preventing linker errors if you accidentally mix them.

---

## Conditional Compilation

Large portions of the library can be compiled out:

| Macro | What it removes |
|-------|----------------|
| `TOML_ENABLE_PARSER=0` | All parsing functions, `parse_error`, `parse_result`, parser implementation |
| `TOML_ENABLE_FORMATTERS=0` | All formatter classes, `format_flags` |
| `TOML_HEADER_ONLY=0` | Switches to compiled mode (link against `toml.cpp`) |

This enables minimal builds for projects that only need, say, programmatic TOML construction and serialization (no parsing).

---

## Related Documentation

- [node-system.md](node-system.md) — Detailed node API reference
- [tables.md](tables.md) — Table operations in depth
- [arrays.md](arrays.md) — Array operations in depth
- [values.md](values.md) — Value template details
- [parsing.md](parsing.md) — Parser behavior and error handling
- [formatting.md](formatting.md) — Formatter usage and customization
