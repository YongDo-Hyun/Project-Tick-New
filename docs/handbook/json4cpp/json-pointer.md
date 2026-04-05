# json4cpp — JSON Pointer (RFC 6901)

## Overview

JSON Pointer (RFC 6901) provides a string syntax for identifying a specific
value within a JSON document. The library implements this as the
`json_pointer` class template, defined in
`include/nlohmann/detail/json_pointer.hpp`.

```cpp
template<typename RefStringType>
class json_pointer
{
    friend class basic_json;

    std::vector<string_t> reference_tokens;  // parsed path segments
};
```

The default alias is:

```cpp
using json_pointer = json_pointer<std::string>;
```

## Syntax

A JSON Pointer is a string of zero or more tokens separated by `/`:

```
""              → whole document
"/foo"          → key "foo" in root object
"/foo/0"        → first element of array at key "foo"
"/a~1b"         → key "a/b" (escaped /)
"/m~0n"         → key "m~n" (escaped ~)
```

### Escape Sequences

| Sequence | Represents |
|---|---|
| `~0` | `~` |
| `~1` | `/` |

Escaping is applied **before** splitting (per RFC 6901 §3).

## Construction

### From String

```cpp
json_pointer(const string_t& s = "");
```

Parses the pointer string and populates `reference_tokens`. Throws
`parse_error::107` if the string is not a valid JSON Pointer (e.g.,
a non-empty string that doesn't start with `/`):

```cpp
json_pointer ptr("/foo/bar/0");

// Invalid:
// json_pointer ptr("foo");  // parse_error::107 — must start with /
```

### User-Defined Literal

```cpp
using namespace nlohmann::literals;

auto ptr = "/server/host"_json_pointer;
```

## Accessing Values

### `operator[]` with Pointer

```cpp
json j = {{"server", {{"host", "localhost"}, {"port", 8080}}}};

j["/server/host"_json_pointer];     // "localhost"
j["/server/port"_json_pointer];     // 8080
j["/server"_json_pointer];          // {"host":"localhost","port":8080}
```

### `at()` with Pointer

```cpp
json j = {{"a", {{"b", 42}}}};

j.at("/a/b"_json_pointer);         // 42
j.at("/a/missing"_json_pointer);   // throws out_of_range::403
```

### `value()` with Pointer

```cpp
json j = {{"timeout", 30}};

j.value("/timeout"_json_pointer, 60);    // 30
j.value("/retries"_json_pointer, 3);     // 3 (key not found, returns default)
```

### `contains()` with Pointer

```cpp
json j = {{"a", {{"b", 42}}}};

j.contains("/a/b"_json_pointer);    // true
j.contains("/a/c"_json_pointer);    // false
j.contains("/x"_json_pointer);      // false
```

## Pointer Manipulation

### `to_string()`

```cpp
string_t to_string() const;
```

Reconstructs the pointer string with proper escaping:

```cpp
json_pointer ptr("/a~1b/0");
ptr.to_string();  // "/a~1b/0"
```

### `operator string_t()`

Implicit conversion to string (same as `to_string()`).

### `operator/=` — Append Token

```cpp
json_pointer& operator/=(const string_t& token);
json_pointer& operator/=(std::size_t array_index);
```

Appends a reference token:

```cpp
json_pointer ptr("/a");
ptr /= "b";    // "/a/b"
ptr /= 0;      // "/a/b/0"
```

### `operator/` — Concatenate

```cpp
friend json_pointer operator/(const json_pointer& lhs, const string_t& token);
friend json_pointer operator/(const json_pointer& lhs, std::size_t array_index);
friend json_pointer operator/(const json_pointer& lhs, const json_pointer& rhs);
```

```cpp
auto ptr = "/a"_json_pointer / "b" / 0;  // "/a/b/0"
auto combined = "/a"_json_pointer / "/b/c"_json_pointer;  // "/a/b/c"
```

### `parent_pointer()`

```cpp
json_pointer parent_pointer() const;
```

Returns the parent pointer (all tokens except the last):

```cpp
auto ptr = "/a/b/c"_json_pointer;
ptr.parent_pointer().to_string();  // "/a/b"

auto root = ""_json_pointer;
root.parent_pointer().to_string();  // "" (root's parent is root)
```

### `back()`

```cpp
const string_t& back() const;
```

Returns the last reference token:

```cpp
auto ptr = "/a/b/c"_json_pointer;
ptr.back();  // "c"
```

Throws `out_of_range::405` if the pointer is empty (root).

### `push_back()`

```cpp
void push_back(const string_t& token);
void push_back(string_t&& token);
```

Appends a token:

```cpp
json_pointer ptr;
ptr.push_back("a");
ptr.push_back("b");
ptr.to_string();  // "/a/b"
```

### `pop_back()`

```cpp
void pop_back();
```

Removes the last token:

```cpp
auto ptr = "/a/b/c"_json_pointer;
ptr.pop_back();
ptr.to_string();  // "/a/b"
```

Throws `out_of_range::405` if the pointer is empty.

### `empty()`

```cpp
bool empty() const noexcept;
```

Returns `true` if the pointer has no reference tokens (i.e., it refers to
the whole document):

```cpp
json_pointer("").empty();      // true  (root pointer)
json_pointer("/a").empty();    // false
```

## Array Indexing

JSON Pointer uses string tokens for array indices. The token `"0"` refers
to the first element, `"1"` to the second, etc.:

```cpp
json j = {"a", "b", "c"};

j["/0"_json_pointer];  // "a"
j["/1"_json_pointer];  // "b"
j["/2"_json_pointer];  // "c"
```

### The `-` Token

The special token `-` refers to the "past-the-end" position in an array.
It can be used with `operator[]` to **append** to an array:

```cpp
json j = {1, 2, 3};
j["/-"_json_pointer] = 4;
// j is now [1, 2, 3, 4]
```

Using `-` with `at()` throws `out_of_range::402` since there's no element
at that position.

## `flatten()` and `unflatten()`

### `flatten()`

```cpp
basic_json flatten() const;
```

Converts a nested JSON value into a flat object where each key is a JSON
Pointer and each value is a primitive:

```cpp
json j = {
    {"name", "alice"},
    {"address", {
        {"city", "wonderland"},
        {"zip", "12345"}
    }},
    {"scores", {90, 85, 92}}
};

json flat = j.flatten();
// {
//     "/name": "alice",
//     "/address/city": "wonderland",
//     "/address/zip": "12345",
//     "/scores/0": 90,
//     "/scores/1": 85,
//     "/scores/2": 92
// }
```

### `unflatten()`

```cpp
basic_json unflatten() const;
```

The inverse of `flatten()`. Reconstructs a nested structure from a flat
pointer-keyed object:

```cpp
json flat = {
    {"/a/b", 1},
    {"/a/c", 2},
    {"/d", 3}
};

json nested = flat.unflatten();
// {"a": {"b": 1, "c": 2}, "d": 3}
```

Throws `type_error::314` if a value is not primitive, or
`type_error::315` if values at a path conflict (e.g., both
`/a` and `/a/b` have values).

### Roundtrip

```cpp
json j = /* any JSON value */;
assert(j == j.flatten().unflatten());
```

Note: `unflatten()` cannot reconstruct arrays from flattened form since
numeric keys (`/0`, `/1`) become object keys. The result will have
object-typed containers where the original had arrays.

## Internal Implementation

### Token Resolution

The `get_checked()` and `get_unchecked()` methods resolve a pointer
against a JSON value by walking through the reference tokens:

```cpp
// Simplified logic
BasicJsonType* ptr = &value;
for (const auto& token : reference_tokens) {
    if (ptr->is_object()) {
        ptr = &ptr->at(token);
    } else if (ptr->is_array()) {
        ptr = &ptr->at(std::stoi(token));
    }
}
return *ptr;
```

### Error IDs

| ID | Condition |
|---|---|
| `parse_error::107` | Invalid pointer syntax |
| `out_of_range::401` | Array index out of range |
| `out_of_range::402` | Array index `-` used with `at()` |
| `out_of_range::403` | Key not found in object |
| `out_of_range::404` | Unresolved reference token |
| `out_of_range::405` | `back()` / `pop_back()` on empty pointer |
