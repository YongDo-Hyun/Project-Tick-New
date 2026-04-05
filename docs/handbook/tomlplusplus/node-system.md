# toml++ — Node System

## Overview

The node system is the core of toml++'s data model. Every element in a TOML document — tables, arrays, and leaf values — is represented as a `toml::node`. This document covers the base class interface, `node_view` for safe access, type checking mechanisms, value retrieval strategies, and visitation patterns.

---

## `toml::node` — The Base Class

`toml::node` is an abstract base class (`TOML_ABSTRACT_INTERFACE`) declared in `include/toml++/impl/node.hpp`. It cannot be instantiated directly; only its derived classes (`table`, `array`, `value<T>`) can.

### Source Tracking

Every node stores a `source_region` tracking its origin in the parsed document:

```cpp
class node
{
  private:
    source_region source_{};

  public:
    const source_region& source() const noexcept;
};
```

For programmatically-constructed nodes, `source()` returns a default-constructed region (all zeros). For parsed nodes, it contains the file path and begin/end line/column.

### Lifetime

```cpp
  protected:
    node() noexcept;
    node(const node&) noexcept;       // copies source_region
    node(node&&) noexcept;            // moves source_region
    node& operator=(const node&) noexcept;
    node& operator=(node&&) noexcept;

  public:
    virtual ~node() noexcept;
```

Constructors are protected — you create nodes by constructing `table`, `array`, or `value<T>` objects.

---

## Type Checking

### Virtual Type Checks

Every `node` provides a full set of virtual type-checking methods:

```cpp
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
```

`is_number()` returns `true` for both integers and floating-point values.

`is_array_of_tables()` returns `true` only for arrays where every element is a table.

### Template Type Check: `is<T>()`

```cpp
template <typename T>
bool is() const noexcept;
```

Accepts any TOML node or value type. Uses `if constexpr` internally to dispatch:

```cpp
node.is<toml::table>()      // equivalent to node.is_table()
node.is<toml::array>()      // equivalent to node.is_array()
node.is<std::string>()      // equivalent to node.is_string()
node.is<int64_t>()          // equivalent to node.is_integer()
node.is<double>()           // equivalent to node.is_floating_point()
node.is<bool>()             // equivalent to node.is_boolean()
node.is<toml::date>()       // equivalent to node.is_date()
node.is<toml::time>()       // equivalent to node.is_time()
node.is<toml::date_time>()  // equivalent to node.is_date_time()
```

You can also use the wrapped `value<T>` type:
```cpp
node.is<toml::value<int64_t>>()  // same as node.is<int64_t>()
```

The `impl::unwrap_node<T>` trait unwraps `value<T>` → `T` and `node_view<T>` → `T`.

### Compile-Time Type Traits

The `toml` namespace provides type traits usable with `if constexpr`:

```cpp
// Type traits for use in generic/template code
toml::is_table<decltype(val)>           // true if val is table or node_view of table
toml::is_array<decltype(val)>           // true if val is array or node_view of array
toml::is_string<decltype(val)>          // true if val is value<std::string>
toml::is_integer<decltype(val)>         // true if val is value<int64_t>
toml::is_floating_point<decltype(val)>  // true if val is value<double>
toml::is_number<decltype(val)>          // integer or floating-point
toml::is_boolean<decltype(val)>         // true if val is value<bool>
toml::is_date<decltype(val)>            // true if val is value<date>
toml::is_time<decltype(val)>            // true if val is value<time>
toml::is_date_time<decltype(val)>       // true if val is value<date_time>
toml::is_value<T>                       // true for any native value type
toml::is_container<T>                   // true for table or array
```

These are critical for `for_each()` visitors:

```cpp
tbl.for_each([](auto& key, auto& value)
{
    if constexpr (toml::is_string<decltype(value)>)
        std::cout << key << " is a string: " << value.get() << "\n";
    else if constexpr (toml::is_integer<decltype(value)>)
        std::cout << key << " is an integer: " << value.get() << "\n";
    else if constexpr (toml::is_table<decltype(value)>)
        std::cout << key << " is a table with " << value.size() << " entries\n";
});
```

### `node_type` Enum

Runtime type identification uses the `node_type` enum:

```cpp
enum class node_type : uint8_t
{
    none,            // sentinel / empty
    table,
    array,
    string,
    integer,
    floating_point,
    boolean,
    date,
    time,
    date_time
};
```

Usage:
```cpp
switch (node.type())
{
    case toml::node_type::string: /* ... */ break;
    case toml::node_type::table:  /* ... */ break;
    // ...
}
```

`node_type` is streamable:
```cpp
std::cout << node.type() << "\n";  // prints "string", "integer", etc.
```

---

## Type Casts: `as<T>()` and Friends

### Virtual Cast Methods

```cpp
// Return pointer if this node IS that type, nullptr otherwise
virtual table* as_table() noexcept = 0;
virtual array* as_array() noexcept = 0;
virtual toml::value<std::string>* as_string() noexcept = 0;
virtual toml::value<int64_t>* as_integer() noexcept = 0;
virtual toml::value<double>* as_floating_point() noexcept = 0;
virtual toml::value<bool>* as_boolean() noexcept = 0;
virtual toml::value<date>* as_date() noexcept = 0;
virtual toml::value<time>* as_time() noexcept = 0;
virtual toml::value<date_time>* as_date_time() noexcept = 0;
// + const overloads
```

Each derived class implements these: `table::as_table()` returns `this`, all others return `nullptr`; `value<int64_t>::as_integer()` returns `this`, all others return `nullptr`.

### Template Cast: `as<T>()`

```cpp
template <typename T>
impl::wrap_node<T>* as() noexcept;

template <typename T>
const impl::wrap_node<T>* as() const noexcept;
```

Dispatches to the appropriate `as_*()` method. `impl::wrap_node<T>` wraps native types in `value<T>`:
- `as<int64_t>()` → `value<int64_t>*` (via `as_integer()`)
- `as<toml::table>()` → `table*` (via `as_table()`)
- `as<toml::value<int64_t>>()` → `value<int64_t>*` (same as above)

Usage:
```cpp
if (auto* tbl = node.as<toml::table>())
    std::cout << "Table with " << tbl->size() << " entries\n";

if (auto* val = node.as<int64_t>())
    std::cout << "Integer: " << val->get() << "\n";
```

### Reference Access: `ref<T>()`

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

Returns a **direct reference** to the underlying value. Unlike `as<T>()`, this does not return a pointer and does not check the type at runtime — it is **undefined behavior** to call `ref<T>()` with the wrong type. It asserts in debug builds.

```cpp
// Only safe if you KNOW the type
int64_t& val = node.ref<int64_t>();
```

---

## Value Retrieval

### `value<T>()` — Permissive Retrieval

```cpp
template <typename T>
optional<T> value() const noexcept(...);
```

Returns the node's value, allowing type conversions:

| Source Type | Target Type | Behavior |
|-------------|-------------|----------|
| `int64_t` | `int64_t` | Exact |
| `int64_t` | `double` | Converts |
| `int64_t` | `int32_t` | Converts if lossless (range check) |
| `int64_t` | `uint32_t` | Converts if lossless (range check) |
| `double` | `double` | Exact |
| `double` | `int64_t` | No (returns empty) |
| `bool` | `bool` | Exact |
| `bool` | `int64_t` | Converts (0 or 1) |
| `std::string` | `std::string_view` | Returns view |
| `std::string` | `std::string` | Returns copy |
| `date` | `date` | Exact |
| `time` | `time` | Exact |
| `date_time` | `date_time` | Exact |

```cpp
auto tbl = toml::parse("val = 42");

// These all work:
auto as_i64 = tbl["val"].value<int64_t>();    // 42
auto as_dbl = tbl["val"].value<double>();      // 42.0
auto as_i32 = tbl["val"].value<int32_t>();     // 42
auto as_u16 = tbl["val"].value<uint16_t>();    // 42

// Returns empty:
auto as_str = tbl["val"].value<std::string>(); // nullopt (int != string)
```

### `value_exact<T>()` — Strict Retrieval

```cpp
template <typename T>
optional<T> value_exact() const noexcept(...);
```

Only returns a value if the node's native type matches exactly:

```cpp
auto tbl = toml::parse("val = 42");

auto exact = tbl["val"].value_exact<int64_t>();   // 42
auto wrong = tbl["val"].value_exact<double>();     // nullopt (42 is integer, not float)
auto wrong2 = tbl["val"].value_exact<int32_t>();   // nullopt (native type is int64_t)
```

Allowed target types for `value_exact<T>()`:
- Native TOML types: `std::string`, `int64_t`, `double`, `bool`, `date`, `time`, `date_time`
- Lossless representations: `std::string_view`, `const char*` (for strings), `std::wstring` (Windows)

### `value_or()` — Retrieval with Default

```cpp
template <typename T>
auto value_or(T&& default_value) const noexcept(...);
```

Returns the value if the node contains a compatible type, otherwise returns the default. The return type matches the default value's type.

```cpp
int64_t port = tbl["port"].value_or(8080);          // 8080 if missing
std::string_view host = tbl["host"].value_or("localhost"sv);

// Works safely on missing paths:
bool flag = tbl["section"]["missing"]["deep"].value_or(false);
```

---

## `toml::node_view<T>` — Safe Optional Node Access

### Purpose

`node_view` wraps a `node*` (possibly null) and provides the full node interface with null safety. It's what `table::operator[]` returns.

### Template Parameter

```cpp
template <typename ViewedType>
class node_view
{
    static_assert(impl::is_one_of<ViewedType, toml::node, const toml::node>);
    // ...
    mutable viewed_type* node_ = nullptr;
};
```

- `node_view<node>` — mutable view
- `node_view<const node>` — const view

### Construction

```cpp
node_view() noexcept = default;                     // empty view
explicit node_view(viewed_type* node) noexcept;     // from pointer
explicit node_view(viewed_type& node) noexcept;     // from reference
```

### Boolean Conversion

```cpp
explicit operator bool() const noexcept;  // true if node_ != nullptr
```

### Chained Subscript

The key design feature — subscript returns another `node_view`, enabling safe deep access:

```cpp
// operator[] on node_view
node_view operator[](std::string_view key) const noexcept;
node_view operator[](size_t index) const noexcept;
node_view operator[](const toml::path& p) const noexcept;
```

If `node_` is null or isn't the right container type, returns an empty `node_view`. This makes arbitrarily deep access safe:

```cpp
// All of these are safe even if intermediate keys don't exist:
auto v1 = tbl["a"]["b"]["c"].value_or(0);
auto v2 = tbl["missing"]["doesn't"]["exist"].value_or("default"sv);
```

### Full Interface Mirror

`node_view` provides all the same methods as `node`:
- Type checks: `is_table()`, `is_string()`, `is<T>()`, etc.
- Type casts: `as_table()`, `as_string()`, `as<T>()`, etc.
- Value retrieval: `value<T>()`, `value_exact<T>()`, `value_or()`
- Source access: `source()`
- Visitation: `visit()`

All safely return defaults/nullptr/empty-optionals when the view is empty.

### `node()` Accessor

```cpp
viewed_type* node() const noexcept;
```

Returns the raw pointer, which may be `nullptr`.

### Printing

```cpp
friend std::ostream& operator<<(std::ostream& os, const node_view& nv);
```

Prints the referenced node's value, or nothing if empty.

---

## Homogeneity Checking

### `is_homogeneous()` — Check Element Type Uniformity

Available on `node`, `table`, and `array`:

```cpp
// With node_type parameter:
virtual bool is_homogeneous(node_type ntype) const noexcept = 0;

// With out-parameter for first mismatch:
virtual bool is_homogeneous(node_type ntype, node*& first_nonmatch) noexcept = 0;

// Template version:
template <typename ElemType = void>
bool is_homogeneous() const noexcept;
```

**Behavior:**
- `node_type::none` → "are all elements the same type?" (any type, as long as consistent)
- Any specific type → "are all elements this type?"
- Returns `false` for empty containers

```cpp
auto arr = toml::array{ 1, 2, 3 };

arr.is_homogeneous(toml::node_type::integer);        // true
arr.is_homogeneous(toml::node_type::string);         // false
arr.is_homogeneous(toml::node_type::none);           // true (all same type)
arr.is_homogeneous<int64_t>();                        // true
arr.is_homogeneous<double>();                         // false
arr.is_homogeneous();                                 // true (void = any consistent type)

// Find the first mismatch:
auto mixed = toml::array{ 1, 2, "oops" };
toml::node* mismatch = nullptr;
if (!mixed.is_homogeneous(toml::node_type::integer, mismatch))
{
    std::cout << "Mismatch at " << mismatch->source() << "\n";
    std::cout << "Type: " << mismatch->type() << "\n";  // "string"
}
```

For `value<T>` nodes, `is_homogeneous()` trivially returns `true` (a single value is always homogeneous with itself).

---

## Visitation with `visit()`

```cpp
template <typename Func>
decltype(auto) visit(Func&& visitor) & noexcept(...);
template <typename Func>
decltype(auto) visit(Func&& visitor) && noexcept(...);
template <typename Func>
decltype(auto) visit(Func&& visitor) const& noexcept(...);
template <typename Func>
decltype(auto) visit(Func&& visitor) const&& noexcept(...);
```

Calls the visitor with the concrete derived type. The visitor must accept all possible types (use a generic lambda or overload set):

```cpp
node.visit([](auto& concrete)
{
    using T = std::remove_cvref_t<decltype(concrete)>;

    if constexpr (std::is_same_v<T, toml::table>)
        std::cout << "table with " << concrete.size() << " keys\n";
    else if constexpr (std::is_same_v<T, toml::array>)
        std::cout << "array with " << concrete.size() << " elements\n";
    else
        std::cout << "value: " << concrete.get() << "\n";
});
```

The visitor receives one of:
- `table&` / `const table&`
- `array&` / `const array&`
- `value<std::string>&` / `const value<std::string>&`
- `value<int64_t>&` / `const value<int64_t>&`
- `value<double>&` / `const value<double>&`
- `value<bool>&` / `const value<bool>&`
- `value<date>&` / `const value<date>&`
- `value<time>&` / `const value<time>&`
- `value<date_time>&` / `const value<date_time>&`

### Return Values

If your visitor returns a value, `visit()` returns it. All branches must return the same type:

```cpp
std::string desc = node.visit([](auto& val) -> std::string
{
    using T = std::remove_cvref_t<decltype(val)>;
    if constexpr (std::is_same_v<T, toml::table>)
        return "table";
    else if constexpr (std::is_same_v<T, toml::array>)
        return "array";
    else
        return "value";
});
```

---

## `for_each()` Iteration

Available on `table` and `array`:

### On Tables

```cpp
template <typename Func>
table& for_each(Func&& visitor) &;
```

The visitor receives `(const toml::key& key, auto& value)` where `value` is the concrete type:

```cpp
tbl.for_each([](const toml::key& key, auto& value)
{
    std::cout << key.str() << " (" << key.source().begin.line << "): ";

    if constexpr (toml::is_string<decltype(value)>)
        std::cout << '"' << value.get() << "\"\n";
    else if constexpr (toml::is_integer<decltype(value)>)
        std::cout << value.get() << "\n";
    else if constexpr (toml::is_table<decltype(value)>)
        std::cout << "{...}\n";
    else
        std::cout << value << "\n";
});
```

### On Arrays

```cpp
template <typename Func>
array& for_each(Func&& visitor) &;
```

The visitor receives `(size_t index, auto& element)` or just `(auto& element)`:

```cpp
arr.for_each([](size_t idx, auto& elem)
{
    std::cout << "[" << idx << "] " << elem << "\n";
});
```

### Early Exit

On compilers without the GCC 7 bug (`TOML_RETURN_BOOL_FROM_FOR_EACH_BROKEN == 0`), your visitor can return `bool` to stop iteration early:

```cpp
tbl.for_each([](const toml::key& key, auto& value) -> bool
{
    if (key.str() == "stop_here")
        return false;  // stop iteration
    std::cout << key << "\n";
    return true;  // continue
});
```

---

## `impl::unwrap_node<T>` and `impl::wrap_node<T>`

These internal type traits handle the mapping between user-facing types and internal node types:

- `unwrap_node<value<int64_t>>` → `int64_t`
- `unwrap_node<int64_t>` → `int64_t` (no change)
- `unwrap_node<node_view<node>>` → `node` (extracted viewed type)
- `wrap_node<int64_t>` → `value<int64_t>`
- `wrap_node<table>` → `table` (no change)
- `wrap_node<array>` → `array` (no change)

These ensure that `as<int64_t>()` returns `value<int64_t>*` and `as<table>()` returns `table*`.

---

## Node Comparison

Nodes support equality comparison:

```cpp
// Same type and value → equal
toml::value<int64_t> a(42);
toml::value<int64_t> b(42);
bool eq = (a == b);  // true

// Cross-type comparison is always false
toml::value<int64_t> i(42);
toml::value<double> d(42.0);
bool eq2 = (i == d);  // false (different node types)

// Tables and arrays compare structurally (deep equality)
```

---

## Summary: Choosing the Right Accessor

| Need | Method | Returns | Null Safe |
|------|--------|---------|-----------|
| Check if key exists | `tbl["key"]` then `operator bool()` | `node_view` | Yes |
| Get value or default | `value_or(default)` | Value type | Yes |
| Get value, might be absent | `value<T>()` | `optional<T>` | Yes |
| Get exact-type value | `value_exact<T>()` | `optional<T>` | Yes |
| Get typed pointer | `as<T>()` | `T*` or `nullptr` | Yes |
| Direct reference (unsafe) | `ref<T>()` | `T&` | No (UB if wrong type) |
| Raw node pointer | `get(key)` | `node*` | Yes (returns null) |
| Typed node pointer | `get_as<T>(key)` | `wrap_node<T>*` | Yes (returns null) |

---

## Related Documentation

- [architecture.md](architecture.md) — Overall class hierarchy
- [tables.md](tables.md) — Table-specific operations
- [arrays.md](arrays.md) — Array-specific operations
- [values.md](values.md) — Value template details
