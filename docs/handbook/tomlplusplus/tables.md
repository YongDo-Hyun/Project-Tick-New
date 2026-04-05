# toml++ — Tables

## Overview

`toml::table` is the primary container in toml++. It extends `toml::node` and models an ordered map from `toml::key` objects to child `toml::node` pointers. Every parsed TOML document has a `table` as its root.

Declared in `include/toml++/impl/table.hpp` with implementation in `table.inl`.

---

## Internal Storage

```cpp
class table : public node
{
  private:
    using map_type = std::map<toml::key, impl::node_ptr, std::less<>>;
    map_type map_;
    bool inline_ = false;
};
```

- **`map_type`** = `std::map<toml::key, std::unique_ptr<node>, std::less<>>`
- **`std::less<>`** enables heterogeneous lookup — you can search by `std::string_view` without constructing a `toml::key`
- **`inline_`** controls whether the table serializes as `{ a = 1, b = 2 }` (inline) or with `[section]` headers (non-inline)
- Ownership: the table owns all child nodes via `unique_ptr`

---

## Construction

### Default Construction

```cpp
toml::table tbl;  // empty table
```

### Initializer List Construction

```cpp
auto tbl = toml::table{
    { "name", "toml++" },
    { "version", 3 },
    { "features", toml::array{ "parsing", "serialization" } },
    { "metadata", toml::table{
        { "author", "Mark Gillard" },
        { "license", "MIT" }
    }}
};
```

This uses `impl::table_init_pair`:
```cpp
struct table_init_pair
{
    mutable toml::key key;
    mutable node_ptr value;

    template <typename K, typename V>
    table_init_pair(K&& k, V&& v, value_flags flags = preserve_source_value_flags);
};
```

Values are converted to nodes via `impl::make_node()`, which handles:
- Native types (`int`, `double`, `const char*`, etc.) → `value<T>`
- `toml::array` → `array` (moved)
- `toml::table` → `table` (moved)

### Copy and Move

```cpp
toml::table copy(original_table);    // deep copy — all child nodes are cloned
toml::table moved(std::move(tbl));   // move — no allocation, transfers ownership
```

Copy is deep: every child node in the tree is recursively copied.

---

## Iterators

### Types

```cpp
using table_iterator = impl::table_iterator<false>;
using const_table_iterator = impl::table_iterator<true>;
```

`table_iterator` is a **BidirectionalIterator**. Dereferencing yields `table_proxy_pair`:

```cpp
template <bool IsConst>
struct table_proxy_pair
{
    using value_type = std::conditional_t<IsConst, const node, node>;
    const toml::key& first;
    value_type& second;
};
```

The `unique_ptr` layer is hidden — you get `(const key&, node&)` pairs.

### Iterator Methods

```cpp
iterator begin() noexcept;
iterator end() noexcept;
const_iterator begin() const noexcept;
const_iterator end() const noexcept;
const_iterator cbegin() const noexcept;
const_iterator cend() const noexcept;
```

### Range-Based For

```cpp
for (auto&& [key, value] : tbl)
{
    std::cout << key << " = " << value << "\n";
}
```

Structured bindings work because `table_proxy_pair` has public `first` and `second` members.

### Iterator to Key String

```cpp
for (auto it = tbl.begin(); it != tbl.end(); ++it)
{
    const toml::key& k = it->first;
    toml::node& v = it->second;
    std::cout << k.str() << ": " << v.type() << "\n";
}
```

---

## Capacity

```cpp
size_t size() const noexcept;     // number of key-value pairs
bool empty() const noexcept;      // true if size() == 0
```

---

## Element Access

### `operator[]` — Returns `node_view`

```cpp
node_view<node> operator[](std::string_view key) noexcept;
node_view<const node> operator[](std::string_view key) const noexcept;
```

Returns a `node_view` that wraps the node at that key, or an empty view if the key doesn't exist. This is the safe, chainable accessor:

```cpp
auto val = tbl["section"]["subsection"]["key"].value_or(42);
```

### `at()` — Bounds-Checked Access

```cpp
node& at(std::string_view key);
const node& at(std::string_view key) const;
```

Returns a reference to the node at the key. Throws `std::out_of_range` if the key doesn't exist.

### `get()` — Raw Pointer Access

```cpp
node* get(std::string_view key) noexcept;
const node* get(std::string_view key) const noexcept;
```

Returns a pointer to the node, or `nullptr` if not found:

```cpp
if (auto* n = tbl.get("name"))
{
    std::cout << "Found: " << *n << "\n";
}
```

### `get_as<T>()` — Typed Pointer Access

```cpp
template <typename T>
impl::wrap_node<T>* get_as(std::string_view key) noexcept;

template <typename T>
const impl::wrap_node<T>* get_as(std::string_view key) const noexcept;
```

Combines `get()` and `as<T>()`:

```cpp
if (auto* val = tbl.get_as<std::string>("name"))
    std::cout << "Name: " << val->get() << "\n";

if (auto* sub = tbl.get_as<toml::table>("database"))
    std::cout << "Database has " << sub->size() << " keys\n";
```

### `contains()` — Key Existence Check

```cpp
bool contains(std::string_view key) const noexcept;
```

```cpp
if (tbl.contains("database"))
    std::cout << "Has database config\n";
```

---

## Insertion

### `insert()` — Insert If Not Present

```cpp
template <typename KeyType, typename ValueType>
std::pair<iterator, bool> insert(KeyType&& key, ValueType&& val,
                                  value_flags flags = preserve_source_value_flags);
```

Inserts a new key-value pair only if the key doesn't already exist. Returns `(iterator, true)` on success, `(iterator_to_existing, false)` if the key was already present:

```cpp
auto [it, inserted] = tbl.insert("name", "toml++");
if (inserted)
    std::cout << "Inserted: " << it->second << "\n";
else
    std::cout << "Key already exists\n";
```

### `insert_or_assign()` — Insert or Replace

```cpp
template <typename KeyType, typename ValueType>
std::pair<iterator, bool> insert_or_assign(KeyType&& key, ValueType&& val,
                                            value_flags flags = preserve_source_value_flags);
```

Always succeeds — inserts if new, replaces if existing:

```cpp
tbl.insert_or_assign("version", 4);  // replaces any existing "version"
```

### `emplace<T>()` — Construct In Place

```cpp
template <typename ValueType, typename KeyType, typename... Args>
std::pair<iterator, bool> emplace(KeyType&& key, Args&&... args);
```

Constructs a new node in place if the key doesn't exist:

```cpp
tbl.emplace<std::string>("greeting", "Hello, World!");
tbl.emplace<toml::array>("empty_list");
tbl.emplace<toml::table>("empty_section");
```

---

## Removal

### `erase()` — By Key

```cpp
size_t erase(std::string_view key) noexcept;
```

Returns 1 if the key was found and removed, 0 otherwise:

```cpp
tbl.erase("deprecated_key");
```

### `erase()` — By Iterator

```cpp
iterator erase(iterator pos) noexcept;
iterator erase(const_iterator pos) noexcept;
iterator erase(const_iterator first, const_iterator last) noexcept;
```

```cpp
auto it = tbl.find("old_key");
if (it != tbl.end())
    tbl.erase(it);
```

### `clear()`

```cpp
void clear() noexcept;
```

Removes all key-value pairs.

---

## Search

### `find()`

```cpp
iterator find(std::string_view key) noexcept;
const_iterator find(std::string_view key) const noexcept;
```

Returns an iterator to the key-value pair, or `end()` if not found.

### `lower_bound()` / `upper_bound()` / `equal_range()`

These operate on the underlying `std::map` with heterogeneous lookup:

```cpp
iterator lower_bound(std::string_view key) noexcept;
iterator upper_bound(std::string_view key) noexcept;
std::pair<iterator, iterator> equal_range(std::string_view key) noexcept;
// + const overloads
```

---

## Metadata

### `is_inline()`

```cpp
bool is_inline() const noexcept;
void is_inline(bool val) noexcept;
```

Controls inline serialization. When `true`, the table formats as `{ a = 1, b = 2 }` instead of using `[section]` headers:

```cpp
auto tbl = toml::table{
    { "a", 1 },
    { "b", 2 },
    { "nested", toml::table{ { "c", 3 } } }
};

std::cout << tbl << "\n";
// Output:
// a = 1
// b = 2
//
// [nested]
// c = 3

tbl.is_inline(true);
std::cout << tbl << "\n";
// Output:
// { a = 1, b = 2, nested = { c = 3 } }
```

Runtime-constructed tables default to non-inline. The parser sets `is_inline(true)` for tables parsed from inline syntax.

---

## `for_each()` — Type-Safe Iteration

```cpp
template <typename Func>
table& for_each(Func&& visitor) &;
```

Visits each key-value pair, passing the value as its concrete type:

```cpp
tbl.for_each([](const toml::key& key, auto& value)
{
    std::cout << key << ": ";

    using value_type = std::remove_cvref_t<decltype(value)>;
    if constexpr (std::is_same_v<value_type, toml::table>)
        std::cout << "table (" << value.size() << " entries)\n";
    else if constexpr (std::is_same_v<value_type, toml::array>)
        std::cout << "array (" << value.size() << " elements)\n";
    else
        std::cout << value.get() << "\n";
});
```

The visitor is instantiated for all 9 possible value types (table, array, + 7 value types).

---

## Path-Based Access

### `at_path()` Member

```cpp
node_view<node> at_path(std::string_view path) noexcept;
node_view<const node> at_path(std::string_view path) const noexcept;
node_view<node> at_path(const toml::path& path) noexcept;
```

Resolves dot-separated paths with array indices:

```cpp
auto tbl = toml::parse(R"(
    [database]
    servers = [
        { host = "alpha", port = 5432 },
        { host = "beta", port = 5433 }
    ]
)");

std::cout << tbl.at_path("database.servers[0].host") << "\n";  // "alpha"
std::cout << tbl.at_path("database.servers[1].port") << "\n";  // 5433
```

### `operator[]` with `toml::path`

```cpp
node_view<node> operator[](const toml::path& path) noexcept;
```

```cpp
toml::path p("database.servers[0].host");
std::cout << tbl[p] << "\n";  // "alpha"
```

---

## Comparison

### Equality

```cpp
friend bool operator==(const table& lhs, const table& rhs) noexcept;
friend bool operator!=(const table& lhs, const table& rhs) noexcept;
```

Deep structural equality: two tables are equal if they have the same keys with equal values. Source regions and inline-ness are not compared.

---

## Printing

Tables are streamable via the default `toml_formatter`:

```cpp
std::cout << tbl << "\n";
```

Equivalent to:
```cpp
std::cout << toml::toml_formatter{ tbl } << "\n";
```

---

## Type Identity

`table` overrides all type-check virtuals from `node`:

```cpp
node_type type() const noexcept final;        // returns node_type::table
bool is_table() const noexcept final;          // returns true
bool is_array() const noexcept final;          // returns false
bool is_value() const noexcept final;          // returns false
bool is_string() const noexcept final;         // returns false
// ... all other is_*() return false

table* as_table() noexcept final;              // returns this
const table* as_table() const noexcept final;  // returns this
// ... all other as_*() return nullptr
```

---

## Windows Compatibility

When `TOML_ENABLE_WINDOWS_COMPAT` is enabled, additional overloads accept `std::wstring_view` for key parameters:

```cpp
node* get(std::wstring_view key);
bool contains(std::wstring_view key) const;
node_view<node> operator[](std::wstring_view key) noexcept;
// etc.
```

Wide strings are internally narrowed via `impl::narrow()`.

---

## Complete Example

```cpp
#include <toml++/toml.hpp>
#include <iostream>

int main()
{
    // Build a table programmatically
    auto config = toml::table{
        { "app", toml::table{
            { "name", "MyApp" },
            { "version", 2 }
        }},
        { "features", toml::array{ "auth", "logging" } }
    };

    // Navigate
    std::cout << "App: " << config["app"]["name"].value_or("?"sv) << "\n";

    // Insert
    config["app"].as_table()->insert("debug", false);

    // Modify
    config.insert_or_assign("features",
        toml::array{ "auth", "logging", "metrics" });

    // Check
    if (config.contains("app"))
    {
        auto* app = config.get_as<toml::table>("app");
        std::cout << "App table has " << app->size() << " keys\n";
    }

    // Iterate
    for (auto&& [key, value] : config)
    {
        std::cout << key << " (" << value.type() << ")\n";
    }

    // Serialize
    std::cout << "\n" << config << "\n";

    return 0;
}
```

---

## Related Documentation

- [node-system.md](node-system.md) — Base node interface
- [arrays.md](arrays.md) — Array container details
- [values.md](values.md) — Value node details
- [path-system.md](path-system.md) — Path-based navigation
