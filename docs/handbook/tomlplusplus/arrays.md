# toml++ — Arrays

## Overview

`toml::array` extends `toml::node` and models a heterogeneous, ordered sequence of TOML nodes. Unlike C++ standard containers that store elements of a single type, a TOML array can contain any mix of value types, sub-tables, and nested arrays.

Declared in `include/toml++/impl/array.hpp` with implementation in `array.inl`.

---

## Internal Storage

```cpp
class array : public node
{
  private:
    std::vector<impl::node_ptr> elems_;
    // impl::node_ptr = std::unique_ptr<node>
};
```

- Each element is owned via `std::unique_ptr<node>`
- The array owns all child nodes — destruction cascades
- Elements can be any `node` subclass: `value<T>`, `table`, or nested `array`

---

## Construction

### Default Construction

```cpp
toml::array arr;  // empty array
```

### Initializer List Construction

```cpp
auto arr = toml::array{ 1, 2, 3 };                    // array of integers
auto mixed = toml::array{ 1, "hello", 3.14, true };   // mixed types
auto nested = toml::array{
    toml::array{ 1, 2 },
    toml::array{ 3, 4 }
};

// Array of tables (array-of-tables syntax in TOML)
auto aot = toml::array{
    toml::table{ { "name", "Alice" }, { "age", 30 } },
    toml::table{ { "name", "Bob" }, { "age", 25 } }
};
```

Values are converted to nodes via `impl::make_node()`:
- `int`, `int64_t` → `value<int64_t>`
- `double`, `float` → `value<double>`
- `const char*`, `std::string` → `value<std::string>`
- `bool` → `value<bool>`
- `toml::date` → `value<date>`
- `toml::time` → `value<time>`
- `toml::date_time` → `value<date_time>`
- `toml::table` → `table` (moved)
- `toml::array` → `array` (moved)

### Copy and Move

```cpp
toml::array copy(original);          // deep copy — all elements cloned
toml::array moved(std::move(arr));   // move — no allocation
```

Copy is recursive: nested tables and arrays are deep-copied.

---

## Iterators

### Types

```cpp
using array_iterator = impl::array_iterator<false>;
using const_array_iterator = impl::array_iterator<true>;
```

`array_iterator` is a **RandomAccessIterator** (unlike `table_iterator` which is Bidirectional). This means it supports arithmetic, comparison, and random access:

```cpp
auto it = arr.begin();
it += 3;                        // jump forward 3
ptrdiff_t diff = arr.end() - it; // distance
bool less = it < arr.end();      // comparison
```

Dereferencing yields `node&` (the `unique_ptr` is hidden):

```cpp
for (auto it = arr.begin(); it != arr.end(); ++it)
{
    toml::node& elem = *it;
    std::cout << elem << "\n";
}
```

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
for (auto& elem : arr)
{
    std::cout << elem.type() << ": " << elem << "\n";
}
```

---

## Capacity

```cpp
size_t size() const noexcept;          // number of elements
bool empty() const noexcept;            // true if size() == 0
size_t capacity() const noexcept;       // reserved capacity
size_t max_size() const noexcept;       // maximum possible size

void reserve(size_t new_cap);           // reserve capacity
void shrink_to_fit();                   // release excess capacity
```

---

## Element Access

### `operator[]` — Unchecked Index Access

```cpp
node& operator[](size_t index) noexcept;
const node& operator[](size_t index) const noexcept;
```

No bounds checking. UB if `index >= size()`.

```cpp
auto arr = toml::array{ 10, 20, 30 };
std::cout << arr[1].value_or(0) << "\n";  // 20
```

### `at()` — Bounds-Checked Access

```cpp
node& at(size_t index);
const node& at(size_t index) const;
```

Throws `std::out_of_range` if `index >= size()`.

### `front()` / `back()`

```cpp
node& front() noexcept;
node& back() noexcept;
const node& front() const noexcept;
const node& back() const noexcept;
```

### `get()` — Pointer Access

```cpp
node* get(size_t index) noexcept;
const node* get(size_t index) const noexcept;
```

Returns `nullptr` if out of bounds (safe alternative to `operator[]`).

### `get_as<T>()` — Typed Pointer Access

```cpp
template <typename T>
impl::wrap_node<T>* get_as(size_t index) noexcept;
```

Returns a typed pointer if the element at `index` exists and matches type `T`:

```cpp
auto arr = toml::array{ "hello", 42, true };

if (auto* s = arr.get_as<std::string>(0))
    std::cout << "String: " << s->get() << "\n";

if (auto* i = arr.get_as<int64_t>(1))
    std::cout << "Integer: " << i->get() << "\n";
```

---

## Insertion

### `push_back()` — Append

```cpp
template <typename T>
decltype(auto) push_back(T&& val, value_flags flags = preserve_source_value_flags);
```

Appends a new element to the end:

```cpp
arr.push_back(42);
arr.push_back("hello");
arr.push_back(toml::table{ { "key", "value" } });
arr.push_back(toml::array{ 1, 2, 3 });
```

Returns a reference to the inserted node.

### `emplace_back<T>()` — Construct at End

```cpp
template <typename T, typename... Args>
decltype(auto) emplace_back(Args&&... args);
```

```cpp
arr.emplace_back<std::string>("constructed in place");
arr.emplace_back<int64_t>(42);
arr.emplace_back<toml::table>();  // empty table
```

### `insert()` — Insert at Position

```cpp
template <typename T>
iterator insert(const_iterator pos, T&& val, value_flags flags = preserve_source_value_flags);
```

```cpp
arr.insert(arr.begin(), "first");         // insert at front
arr.insert(arr.begin() + 2, 42);          // insert at index 2
arr.insert(arr.end(), toml::array{1,2});  // same as push_back
```

### `emplace()` — Construct at Position

```cpp
template <typename T, typename... Args>
iterator emplace(const_iterator pos, Args&&... args);
```

```cpp
arr.emplace<std::string>(arr.begin(), "inserted string");
```

---

## Removal

### `pop_back()`

```cpp
void pop_back() noexcept;
```

Removes the last element.

### `erase()` — By Iterator

```cpp
iterator erase(const_iterator pos) noexcept;
iterator erase(const_iterator first, const_iterator last) noexcept;
```

```cpp
arr.erase(arr.begin());           // remove first
arr.erase(arr.begin(), arr.begin() + 3);  // remove first 3
```

### `clear()`

```cpp
void clear() noexcept;
```

Removes all elements.

### `resize()`

```cpp
void resize(size_t new_size, T&& default_value, value_flags flags = preserve_source_value_flags);
```

If `new_size > size()`, appends copies of `default_value`. If `new_size < size()`, truncates.

### `truncate()`

```cpp
void truncate(size_t new_size);
```

Removes elements beyond `new_size`. If `new_size >= size()`, does nothing.

---

## Array Transformation

### `flatten()` — Flatten Nested Arrays

```cpp
array& flatten() &;
array&& flatten() &&;
```

Recursively flattens nested arrays into a single-level array:

```cpp
auto arr = toml::array{
    1,
    toml::array{ 2, 3 },
    toml::array{ toml::array{ 4, 5 }, 6 }
};

arr.flatten();
// arr is now: [1, 2, 3, 4, 5, 6]
```

Tables within nested arrays are **not** flattened — only arrays are unwrapped.

### `prune()` — Remove Empty Containers

```cpp
array& prune(bool recursive = true) &;
array&& prune(bool recursive = true) &&;
```

Removes empty tables and empty arrays. If `recursive` is true, prunes nested containers first, then removes them if they became empty:

```cpp
auto arr = toml::array{
    1,
    toml::table{},           // empty table
    toml::array{},           // empty array
    toml::array{ toml::table{} }  // array containing empty table
};

arr.prune();
// arr is now: [1]
```

---

## Homogeneity

### Checking Type Uniformity

```cpp
bool is_homogeneous(node_type ntype) const noexcept;
bool is_homogeneous(node_type ntype, node*& first_nonmatch) noexcept;
bool is_homogeneous(node_type ntype, const node*& first_nonmatch) const noexcept;

template <typename ElemType = void>
bool is_homogeneous() const noexcept;
```

```cpp
auto ints = toml::array{ 1, 2, 3 };
auto mixed = toml::array{ 1, "two", 3.0 };

ints.is_homogeneous<int64_t>();  // true
ints.is_homogeneous<double>();   // false
ints.is_homogeneous();           // true (all same type)

mixed.is_homogeneous();          // false
mixed.is_homogeneous(toml::node_type::none);  // false

// Find first mismatch:
toml::node* bad = nullptr;
mixed.is_homogeneous(toml::node_type::integer, bad);
// bad points to the "two" string value
```

**Important:** Empty arrays return `false` for all homogeneity checks — they don't contain "any" type.

### `is_array_of_tables()`

```cpp
bool is_array_of_tables() const noexcept;
```

Returns `true` only if the array is non-empty and every element is a `table`:

```cpp
auto aot = toml::array{
    toml::table{ { "name", "Alice" } },
    toml::table{ { "name", "Bob" } }
};
std::cout << aot.is_array_of_tables() << "\n";  // true

auto mixed = toml::array{ toml::table{}, 42 };
std::cout << mixed.is_array_of_tables() << "\n";  // false
```

---

## `for_each()` — Type-Safe Iteration

```cpp
template <typename Func>
array& for_each(Func&& visitor) &;
template <typename Func>
array&& for_each(Func&& visitor) &&;
template <typename Func>
const array& for_each(Func&& visitor) const&;
```

Iterates elements, calling the visitor with each element in its concrete type. The visitor can accept:
- `(auto& element)` — element only
- `(size_t index, auto& element)` — index + element

```cpp
auto arr = toml::array{ 1, "two", 3.0, true };

arr.for_each([](size_t idx, auto& elem)
{
    using T = std::remove_cvref_t<decltype(elem)>;

    std::cout << "[" << idx << "] ";

    if constexpr (toml::is_integer<T>)
        std::cout << "int: " << elem.get() << "\n";
    else if constexpr (toml::is_string<T>)
        std::cout << "string: " << elem.get() << "\n";
    else if constexpr (toml::is_floating_point<T>)
        std::cout << "float: " << elem.get() << "\n";
    else if constexpr (toml::is_boolean<T>)
        std::cout << "bool: " << elem.get() << "\n";
});
```

Output:
```
[0] int: 1
[1] string: two
[2] float: 3
[3] bool: true
```

### Early Exit (Non-GCC-7)

On supported compilers, returning `bool` from the visitor enables early termination:

```cpp
arr.for_each([](auto& elem) -> bool
{
    if constexpr (toml::is_string<decltype(elem)>)
    {
        std::cout << "Found string: " << elem.get() << "\n";
        return false;  // stop
    }
    return true;  // continue
});
```

---

## Array of Tables (TOML `[[syntax]]`)

In TOML, `[[array_name]]` defines an array of tables:

```toml
[[servers]]
name = "alpha"
ip = "10.0.0.1"

[[servers]]
name = "beta"
ip = "10.0.0.2"
```

This parses to a `table` containing key `"servers"` → `array` → `[table, table]`.

Accessing:
```cpp
auto tbl = toml::parse(/* above TOML */);

if (auto* servers = tbl["servers"].as_array())
{
    for (auto& server_node : *servers)
    {
        auto* server = server_node.as_table();
        if (server)
        {
            auto name = (*server)["name"].value_or(""sv);
            auto ip = (*server)["ip"].value_or(""sv);
            std::cout << name << " @ " << ip << "\n";
        }
    }
}
```

Creating programmatically:
```cpp
auto tbl = toml::table{
    { "servers", toml::array{
        toml::table{ { "name", "alpha" }, { "ip", "10.0.0.1" } },
        toml::table{ { "name", "beta" }, { "ip", "10.0.0.2" } }
    }}
};
```

---

## Comparison

### Equality

```cpp
friend bool operator==(const array& lhs, const array& rhs) noexcept;
friend bool operator!=(const array& lhs, const array& rhs) noexcept;
```

Deep structural equality: same size, same element types, same values in order.

---

## Printing

Arrays are streamable:

```cpp
auto arr = toml::array{ 1, 2, 3, "four" };
std::cout << arr << "\n";
// Output: [1, 2, 3, "four"]
```

The output format depends on the formatter. The default `toml_formatter` uses inline array syntax for simple arrays and multiline for arrays of tables.

---

## Type Identity

```cpp
node_type type() const noexcept final;        // returns node_type::array
bool is_table() const noexcept final;          // returns false
bool is_array() const noexcept final;          // returns true
bool is_value() const noexcept final;          // returns false
// ... all other is_*() return false

array* as_array() noexcept final;              // returns this
const array* as_array() const noexcept final;  // returns this
// ... all other as_*() return nullptr
```

---

## Complete Example

```cpp
#include <toml++/toml.hpp>
#include <iostream>

int main()
{
    // Build an array
    toml::array fruits;
    fruits.push_back("apple");
    fruits.push_back("banana");
    fruits.push_back("cherry");

    // Insert at position
    fruits.insert(fruits.begin() + 1, "blueberry");

    // Iterate
    for (size_t i = 0; i < fruits.size(); i++)
    {
        std::cout << i << ": " << fruits[i].value_or(""sv) << "\n";
    }

    // Check homogeneity
    std::cout << "All strings? " << fruits.is_homogeneous<std::string>() << "\n";

    // Flatten nested arrays
    auto nested = toml::array{
        toml::array{ 1, 2 },
        toml::array{ 3, toml::array{ 4, 5 } }
    };
    nested.flatten();
    std::cout << "Flattened: " << nested << "\n";

    // Array of tables
    auto servers = toml::array{
        toml::table{ { "host", "alpha" }, { "port", 8080 } },
        toml::table{ { "host", "beta" }, { "port", 8081 } }
    };
    std::cout << "Is array of tables? " << servers.is_array_of_tables() << "\n";

    // for_each with type dispatch
    auto mixed = toml::array{ 42, "hello", 3.14 };
    mixed.for_each([](auto& elem)
    {
        if constexpr (toml::is_integer<decltype(elem)>)
            std::cout << "int: " << elem.get() << "\n";
        else if constexpr (toml::is_string<decltype(elem)>)
            std::cout << "str: " << elem.get() << "\n";
        else if constexpr (toml::is_floating_point<decltype(elem)>)
            std::cout << "flt: " << elem.get() << "\n";
    });

    return 0;
}
```

---

## Related Documentation

- [node-system.md](node-system.md) — Base node interface
- [tables.md](tables.md) — Table container details
- [values.md](values.md) — Leaf value details
- [formatting.md](formatting.md) — Array formatting options
