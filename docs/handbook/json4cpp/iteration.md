# json4cpp — Iteration

## Iterator Types

The `basic_json` class provides a full set of iterators modeled after STL
container iterators. All are defined in
`include/nlohmann/detail/iterators/`:

| Type | Class | Header |
|---|---|---|
| `iterator` | `iter_impl<basic_json>` | `iter_impl.hpp` |
| `const_iterator` | `iter_impl<const basic_json>` | `iter_impl.hpp` |
| `reverse_iterator` | `json_reverse_iterator<iterator>` | `json_reverse_iterator.hpp` |
| `const_reverse_iterator` | `json_reverse_iterator<const_iterator>` | `json_reverse_iterator.hpp` |

## `iter_impl` Internals

The `iter_impl<BasicJsonType>` template is the core iterator
implementation. It wraps an `internal_iterator` struct:

```cpp
struct internal_iterator
{
    typename BasicJsonType::object_t::iterator object_iterator;
    typename BasicJsonType::array_t::iterator array_iterator;
    primitive_iterator_t primitive_iterator;
};
```

Only one of these three fields is active at a time, determined by the
`m_object` pointer's `type()`:

- **Object**: uses `object_iterator` (delegates to the underlying map/ordered_map iterator)
- **Array**: uses `array_iterator` (delegates to `std::vector::iterator`)
- **Primitive** (null, boolean, number, string, binary): uses `primitive_iterator_t`

### `primitive_iterator_t`

Primitive types are treated as single-element containers. The
`primitive_iterator_t` is a wrapper around `std::ptrdiff_t`:

- Value `0` → points to the element (equivalent to `begin()`)
- Value `1` → past-the-end (equivalent to `end()`)
- Value `-1` → before-the-begin (sentinel, `end_value`)

```cpp
json j = 42;
for (auto it = j.begin(); it != j.end(); ++it) {
    // executes exactly once, *it == 42
}
```

## Range Functions

### Forward Iteration

```cpp
iterator begin() noexcept;
const_iterator begin() const noexcept;
const_iterator cbegin() const noexcept;

iterator end() noexcept;
const_iterator end() const noexcept;
const_iterator cend() const noexcept;
```

```cpp
json j = {1, 2, 3};
for (auto it = j.begin(); it != j.end(); ++it) {
    std::cout << *it << " ";
}
// Output: 1 2 3
```

### Reverse Iteration

```cpp
reverse_iterator rbegin() noexcept;
const_reverse_iterator rbegin() const noexcept;
const_reverse_iterator crbegin() const noexcept;

reverse_iterator rend() noexcept;
const_reverse_iterator rend() const noexcept;
const_reverse_iterator crend() const noexcept;
```

```cpp
json j = {1, 2, 3};
for (auto it = j.rbegin(); it != j.rend(); ++it) {
    std::cout << *it << " ";
}
// Output: 3 2 1
```

## Range-Based For Loops

### Simple Iteration

```cpp
json j = {"alpha", "beta", "gamma"};
for (const auto& element : j) {
    std::cout << element << "\n";
}
```

### Mutable Iteration

```cpp
json j = {1, 2, 3};
for (auto& element : j) {
    element = element.get<int>() * 2;
}
// j is now [2, 4, 6]
```

### Object Iteration

When iterating over objects, each element is a **value** (not a key-value
pair). Use `it.key()` and `it.value()` on an explicit iterator, or use
`items()`:

```cpp
json j = {{"name", "alice"}, {"age", 30}};

// Method 1: explicit iterator
for (auto it = j.begin(); it != j.end(); ++it) {
    std::cout << it.key() << ": " << it.value() << "\n";
}

// Method 2: range-for gives values only
for (const auto& val : j) {
    // val is the value, key is not accessible
}
```

## `items()` — Key-Value Iteration

```cpp
iteration_proxy<iterator> items() noexcept;
iteration_proxy<const_iterator> items() const noexcept;
```

Returns an `iteration_proxy` that wraps the iterator to provide `key()`
and `value()` accessors in range-based for loops:

```cpp
json j = {{"name", "alice"}, {"age", 30}, {"active", true}};

for (auto& [key, value] : j.items()) {
    std::cout << key << " = " << value << "\n";
}
```

### Array Keys

For arrays, `items()` synthesizes string keys from the index:

```cpp
json j = {"a", "b", "c"};

for (auto& [key, value] : j.items()) {
    std::cout << key << ": " << value << "\n";
}
// Output:
// 0: "a"
// 1: "b"
// 2: "c"
```

### Primitive Keys

For primitives, the key is always `""` (empty string):

```cpp
json j = 42;
for (auto& [key, value] : j.items()) {
    // key == "", value == 42
}
```

## `iteration_proxy` Implementation

Defined in `include/nlohmann/detail/iterators/iteration_proxy.hpp`:

```cpp
template<typename IteratorType>
class iteration_proxy
{
    class iteration_proxy_value
    {
        IteratorType anchor;           // underlying iterator
        std::size_t array_index = 0;   // cached index for arrays
        mutable std::size_t array_index_last = 0;
        mutable string_type array_index_str = "0";

        const string_type& key() const;
        typename IteratorType::reference value() const;
    };

public:
    iteration_proxy_value begin() const noexcept;
    iteration_proxy_value end() const noexcept;
};
```

The `key()` method dispatches based on the value type:
- **Array**: converts `array_index` to string (`std::to_string`)
- **Object**: returns `anchor.key()`
- **Other**: returns empty string

## Structured Bindings

C++17 structured bindings work with `items()`:

```cpp
json j = {{"x", 1}, {"y", 2}};
for (const auto& [key, value] : j.items()) {
    // key is const std::string&, value is const json&
}
```

This is enabled by specializations in `<nlohmann/detail/iterators/iteration_proxy.hpp>`:

```cpp
namespace std {
    template<std::size_t N, typename IteratorType>
    struct tuple_element<N, ::nlohmann::detail::iteration_proxy_value<IteratorType>>;

    template<typename IteratorType>
    struct tuple_size<::nlohmann::detail::iteration_proxy_value<IteratorType>>;
}
```

## Iterator Arithmetic (Arrays Only)

Array iterators support random access:

```cpp
json j = {10, 20, 30, 40, 50};

auto it = j.begin();
it += 2;           // points to 30
auto it2 = it + 1; // points to 40
auto diff = it2 - it;  // 1

j[it - j.begin()];  // equivalent of *it
```

Object iterators support only increment and decrement (bidirectional).

## `json_reverse_iterator`

Extends `std::reverse_iterator` with `key()` and `value()` methods:

```cpp
template<typename Base>
class json_reverse_iterator : public std::reverse_iterator<Base>
{
public:
    // Inherited from std::reverse_iterator:
    // operator*, operator->, operator++, operator--, operator+, operator- ...

    // Added:
    const typename Base::key_type& key() const;
    typename Base::reference value() const;
};
```

```cpp
json j = {{"a", 1}, {"b", 2}, {"c", 3}};
for (auto it = j.rbegin(); it != j.rend(); ++it) {
    std::cout << it.key() << ": " << it.value() << "\n";
}
// Output (reversed iteration order)
```

## Iterator Invalidation

Iterator invalidation follows the rules of the underlying containers:

| Operation | Object (`std::map`) | Array (`std::vector`) |
|---|---|---|
| `push_back()` | Not invalidated | May invalidate all |
| `insert()` | Not invalidated | Invalidates at/after pos |
| `erase()` | Only erased | At/after erased pos |
| `clear()` | All invalidated | All invalidated |
| `operator[]` (new key) | Not invalidated | May invalidate all |

For `ordered_json` (backed by `std::vector`), all iterators may be
invalidated on any insertion/erasure since the ordered_map inherits from
`std::vector`.

## Iterating Null Values

Null values behave as empty containers:

```cpp
json j;  // null
for (const auto& el : j) {
    // never executes
}
assert(j.begin() == j.end());
```

## Complete Example

```cpp
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

int main() {
    json config = {
        {"server", {
            {"host", "localhost"},
            {"port", 8080},
            {"features", {"auth", "logging", "metrics"}}
        }},
        {"debug", false}
    };

    // Iterate top-level keys
    for (auto& [key, value] : config.items()) {
        std::cout << key << " [" << value.type_name() << "]\n";
    }

    // Iterate nested array
    for (const auto& feature : config["server"]["features"]) {
        std::cout << "  feature: " << feature << "\n";
    }

    // Reverse iterate
    auto& features = config["server"]["features"];
    for (auto it = features.rbegin(); it != features.rend(); ++it) {
        std::cout << "  reverse: " << *it << "\n";
    }
}
```
