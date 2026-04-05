# json4cpp — Element Access

## Overview

The `basic_json` class provides several ways to access elements:

| Method | Applicable To | Behaviour on Missing |
|---|---|---|
| `operator[]` | array, object, null | Inserts default (creates if null) |
| `at()` | array, object | Throws `out_of_range` |
| `value()` | object | Returns caller-supplied default |
| `front()` | array, object, scalar | UB if empty |
| `back()` | array, object, scalar | UB if empty |
| `find()` | object | Returns `end()` |
| `contains()` | object | Returns `false` |
| `count()` | object | Returns `0` |

## `operator[]`

### Array Access

```cpp
reference operator[](size_type idx);
const_reference operator[](size_type idx) const;
```

Accesses the element at index `idx`. If the JSON value is **null**, it is
automatically converted to an **array** before accessing:

```cpp
json j;                 // null
j[0] = "first";         // j is now ["first"]
j[1] = "second";        // j is now ["first", "second"]
```

If `idx` is beyond the current array size, the array is extended with null
elements:

```cpp
json j = {1, 2};
j[5] = 99;
// j is now [1, 2, null, null, null, 99]
```

**Warning:** `const` array access does **not** extend the array and has
undefined behavior for out-of-bounds access.

### Object Access

```cpp
reference operator[](const typename object_t::key_type& key);
const_reference operator[](const typename object_t::key_type& key) const;

// C++14 heterogeneous lookup (KeyType template)
template<typename KeyType>
reference operator[](KeyType&& key);
template<typename KeyType>
const_reference operator[](KeyType&& key) const;
```

Accesses the element with key `key`. If the key does not exist in a mutable
context, it is **inserted** with a null value:

```cpp
json j = {{"name", "alice"}};
j["age"] = 30;            // inserts "age"
std::string name = j["name"];

// const access does not insert
const json& cj = j;
// cj["missing"];  // undefined behavior if key doesn't exist
```

If the JSON value is **null**, it is automatically converted to an **object**:

```cpp
json j;              // null
j["key"] = "value";  // j is now {"key": "value"}
```

### String Literal vs. Integer Ambiguity

Be careful with `0`:

```cpp
json j = {{"key", "value"}};
// j[0] — array access (selects first element of object iteration) — NOT recommended
// j["key"] — object access
```

### Using `json::object_t::key_type`

The non-const `operator[]` accepts a `key_type` (default: `std::string`).
The `KeyType` template overloads accept any type that satisfies these
constraints via `detail::is_usable_as_key_type`:

- Must be comparable with `object_comparator_t`
- Not convertible to `basic_json`
- Not a `value_t`
- Not a `BasicJsonType`

## `at()`

### Array Access

```cpp
reference at(size_type idx);
const_reference at(size_type idx) const;
```

Returns a reference to the element at index `idx`. Throws
`json::out_of_range` (id 401) if the index is out of bounds:

```cpp
json j = {1, 2, 3};
j.at(0);  // 1
j.at(3);  // throws out_of_range::401: "array index 3 is out of range"
```

When `JSON_DIAGNOSTIC_POSITIONS` is enabled, the exception includes
byte-offset information.

### Object Access

```cpp
reference at(const typename object_t::key_type& key);
const_reference at(const typename object_t::key_type& key) const;

template<typename KeyType>
reference at(KeyType&& key);
template<typename KeyType>
const_reference at(KeyType&& key) const;
```

Returns a reference to the element with key `key`. Throws
`json::out_of_range` (id 403) if the key is not found:

```cpp
json j = {{"name", "alice"}, {"age", 30}};
j.at("name");     // "alice"
j.at("missing");  // throws out_of_range::403: "key 'missing' not found"
```

### Type Mismatch

Both `at()` overloads throw `json::type_error` (id 304) if the JSON value
is not of the expected type:

```cpp
json j = 42;
j.at(0);       // throws type_error::304: "cannot use at() with number"
j.at("key");   // throws type_error::304: "cannot use at() with number"
```

## `value()`

```cpp
// With default value
ValueType value(const typename object_t::key_type& key, const ValueType& default_value) const;

// With JSON pointer
ValueType value(const json_pointer& ptr, const ValueType& default_value) const;

// KeyType template overloads
template<typename KeyType>
ValueType value(KeyType&& key, const ValueType& default_value) const;
```

Returns the value for a given key or JSON pointer, or `default_value` if
the key/pointer does not resolve. Unlike `operator[]` and `at()`, this
method **never modifies** the JSON value.

```cpp
json j = {{"name", "alice"}, {"age", 30}};

std::string name = j.value("name", "unknown");  // "alice"
std::string addr = j.value("address", "N/A");   // "N/A"
int height = j.value("height", 170);             // 170

// With JSON pointer
int age = j.value("/age"_json_pointer, 0);       // 30
int foo = j.value("/foo"_json_pointer, -1);       // -1
```

Throws `json::type_error` (id 306) if the JSON value is not an object (for
the key overloads) or if the found value cannot be converted to `ValueType`.

### `value()` vs `operator[]`

| Feature | `operator[]` | `value()` |
|---|---|---|
| Modifies on miss | Yes (inserts null) | No |
| Returns | Reference | Value copy |
| Default on miss | null (always) | Caller-specified |
| Applicable to arrays | Yes | No (objects only) |

## `front()` and `back()`

```cpp
reference front();
const_reference front() const;

reference back();
const_reference back() const;
```

Return references to the first/last element. For **arrays**, this is the
first/last element by index. For **objects**, this is the first/last element
by iteration order (which depends on the comparator — insertion order for
`ordered_json`). For **non-compound types**, the value itself is returned
(the JSON value is treated as a single-element container).

```cpp
json j = {1, 2, 3};
j.front();  // 1
j.back();   // 3

json j2 = 42;
j2.front();  // 42
j2.back();   // 42
```

**Warning:** Calling `front()` or `back()` on an empty container is
**undefined behavior** (same as STL containers).

## `find()`

```cpp
iterator find(const typename object_t::key_type& key);
const_iterator find(const typename object_t::key_type& key) const;

template<typename KeyType>
iterator find(KeyType&& key);
template<typename KeyType>
const_iterator find(KeyType&& key) const;
```

Returns an iterator to the element with the given key, or `end()` if not
found. Only works on objects:

```cpp
json j = {{"name", "alice"}, {"age", 30}};

auto it = j.find("name");
if (it != j.end()) {
    std::cout << it.key() << " = " << it.value() << "\n";
}

auto it2 = j.find("missing");
assert(it2 == j.end());
```

For non-objects, `find()` always returns `end()`.

## `contains()`

```cpp
bool contains(const typename object_t::key_type& key) const;

template<typename KeyType>
bool contains(KeyType&& key) const;

// JSON pointer overload
bool contains(const json_pointer& ptr) const;
```

Returns `true` if the key or pointer exists:

```cpp
json j = {{"name", "alice"}, {"address", {{"city", "wonderland"}}}};

j.contains("name");    // true
j.contains("phone");   // false

// JSON pointer — checks nested paths
j.contains("/address/city"_json_pointer);    // true
j.contains("/address/zip"_json_pointer);     // false
```

## `count()`

```cpp
size_type count(const typename object_t::key_type& key) const;

template<typename KeyType>
size_type count(KeyType&& key) const;
```

Returns the number of elements with the given key. Since JSON objects have
unique keys, the result is always `0` or `1`:

```cpp
json j = {{"name", "alice"}};
j.count("name");     // 1
j.count("missing");  // 0
```

## `erase()`

### Erase by Iterator

```cpp
iterator erase(iterator pos);
iterator erase(const_iterator pos);
```

Removes the element at the given iterator position. Returns an iterator to
the element after the erased one:

```cpp
json j = {1, 2, 3, 4, 5};
auto it = j.erase(j.begin() + 2);   // removes 3
// j is now [1, 2, 4, 5], it points to 4
```

### Erase by Iterator Range

```cpp
iterator erase(iterator first, iterator last);
iterator erase(const_iterator first, const_iterator last);
```

Removes all elements in the range `[first, last)`:

```cpp
json j = {1, 2, 3, 4, 5};
j.erase(j.begin() + 1, j.begin() + 3);
// j is now [1, 4, 5]
```

### Erase by Key

```cpp
size_type erase(const typename object_t::key_type& key);

template<typename KeyType>
size_type erase(KeyType&& key);
```

Removes the element with the given key from an object. Returns the number
of elements removed (0 or 1):

```cpp
json j = {{"name", "alice"}, {"age", 30}};
j.erase("age");
// j is now {"name": "alice"}
```

### Erase by Index

```cpp
void erase(const size_type idx);
```

Removes the element at the given index from an array. Throws
`out_of_range::401` if the index is out of range:

```cpp
json j = {"a", "b", "c"};
j.erase(1);
// j is now ["a", "c"]
```

### Erase on Primitive Types

Erasing by iterator on primitive types (number, string, boolean) is
supported only if the iterator points to the single element:

```cpp
json j = 42;
j.erase(j.begin());  // j is now null
```

## `size()`, `empty()`, `max_size()`

```cpp
size_type size() const noexcept;
bool empty() const noexcept;
size_type max_size() const noexcept;
```

| Type | `size()` | `empty()` |
|---|---|---|
| null | 0 | `true` |
| object | number of key-value pairs | `true` if no pairs |
| array | number of elements | `true` if no elements |
| scalar (string, number, boolean, binary) | 1 | `false` |

```cpp
json j_null;
j_null.size();   // 0
j_null.empty();  // true

json j_arr = {1, 2, 3};
j_arr.size();    // 3
j_arr.empty();   // false

json j_str = "hello";
j_str.size();    // 1
j_str.empty();   // false (primitive → always 1)
```

`max_size()` returns the maximum number of elements the container can hold
(delegates to the underlying container's `max_size()` for arrays and
objects; returns 1 for scalars).

## `clear()`

```cpp
void clear() noexcept;
```

Resets the value to a default-constructed value of the same type:

| Type | Result after `clear()` |
|---|---|
| null | null |
| object | `{}` |
| array | `[]` |
| string | `""` |
| boolean | `false` |
| number_integer | `0` |
| number_unsigned | `0` |
| number_float | `0.0` |
| binary | `[]` (empty, no subtype) |

## `push_back()` and `emplace_back()`

### Array Operations

```cpp
void push_back(basic_json&& val);
void push_back(const basic_json& val);

template<typename... Args>
reference emplace_back(Args&&... args);
```

Appends an element at the end:

```cpp
json j = {1, 2, 3};
j.push_back(4);
j.emplace_back(5);
// j is now [1, 2, 3, 4, 5]
```

If the value is `null`, it's first converted to an empty array.

### Object Operations

```cpp
void push_back(const typename object_t::value_type& val);
void push_back(initializer_list_t init);
```

Inserts a key-value pair:

```cpp
json j = {{"a", 1}};
j.push_back({"b", 2});      // initializer_list pair
j.push_back(json::object_t::value_type("c", 3));  // explicit pair
// j is now {"a": 1, "b": 2, "c": 3}
```

### `operator+=`

Alias for `push_back()`:

```cpp
json j = {1, 2};
j += 3;
j += {4, 5};  // pushes an array [4, 5] as a single element
```

## `emplace()`

```cpp
template<typename... Args>
std::pair<iterator, bool> emplace(Args&&... args);
```

For objects, inserts a key-value pair if the key doesn't already exist.
Returns a pair of iterator and bool (whether insertion took place):

```cpp
json j = {{"a", 1}};
auto [it, inserted] = j.emplace("b", 2);
// inserted == true, it points to {"b": 2}
auto [it2, inserted2] = j.emplace("a", 99);
// inserted2 == false, existing value unchanged
```

## `insert()`

### Array Insert

```cpp
iterator insert(const_iterator pos, const basic_json& val);
iterator insert(const_iterator pos, basic_json&& val);
iterator insert(const_iterator pos, size_type cnt, const basic_json& val);
iterator insert(const_iterator pos, const_iterator first, const_iterator last);
iterator insert(const_iterator pos, initializer_list_t ilist);
```

Inserts elements at the given position:

```cpp
json j = {1, 2, 5};
j.insert(j.begin() + 2, 3);
j.insert(j.begin() + 3, 4);
// j is now [1, 2, 3, 4, 5]

// Insert count copies
j.insert(j.end(), 2, 0);
// j is now [1, 2, 3, 4, 5, 0, 0]
```

### Object Insert

```cpp
void insert(const_iterator first, const_iterator last);
```

Inserts elements from another object:

```cpp
json j1 = {{"a", 1}};
json j2 = {{"b", 2}, {"c", 3}};
j1.insert(j2.begin(), j2.end());
// j1 is now {"a": 1, "b": 2, "c": 3}
```

## `update()`

```cpp
void update(const_reference j, bool merge_objects = false);
void update(const_iterator first, const_iterator last, bool merge_objects = false);
```

Updates an object with keys from another object. Existing keys are
**overwritten**:

```cpp
json j1 = {{"a", 1}, {"b", 2}};
json j2 = {{"b", 99}, {"c", 3}};
j1.update(j2);
// j1 is now {"a": 1, "b": 99, "c": 3}
```

When `merge_objects` is `true`, nested objects are merged recursively
instead of being overwritten:

```cpp
json j1 = {{"config", {{"debug", true}, {"port", 8080}}}};
json j2 = {{"config", {{"port", 9090}, {"host", "localhost"}}}};
j1.update(j2, true);
// j1["config"] is now {"debug": true, "port": 9090, "host": "localhost"}
```

## `swap()`

```cpp
void swap(reference other) noexcept;

void swap(array_t& other);
void swap(object_t& other);
void swap(string_t& other);
void swap(binary_t& other);
void swap(typename binary_t::container_type& other);
```

Swaps contents with another value or with a compatible container.
The typed overloads throw `type_error::310` if the types don't match:

```cpp
json j = {1, 2, 3};
std::vector<json> v = {4, 5, 6};
j.swap(v);
// j is now [4, 5, 6], v contains the old j's elements
```
