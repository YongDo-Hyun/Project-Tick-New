# json4cpp — Exception Handling

## Exception Hierarchy

All exceptions derive from `json::exception`, which itself inherits from
`std::exception`. Defined in `include/nlohmann/detail/exceptions.hpp`:

```
std::exception
  └── json::exception
        ├── json::parse_error
        ├── json::invalid_iterator
        ├── json::type_error
        ├── json::out_of_range
        └── json::other_error
```

## Base Class: `json::exception`

```cpp
class exception : public std::exception
{
public:
    const char* what() const noexcept override;
    const int id;           // numeric error identifier

protected:
    exception(int id_, const char* what_arg);

    static std::string name(const std::string& ename, int id_);
    static std::string diagnostics(std::nullptr_t leaf_element);
    static std::string diagnostics(const BasicJsonType* leaf_element);

private:
    std::runtime_error m;   // stores the what() message
};
```

### Error Message Format

```
[json.exception.<type>.<id>] <description>
```

Example:
```
[json.exception.type_error.302] type must be string, but is number
```

### Diagnostics Mode

When `JSON_DIAGNOSTICS` is enabled, error messages include the **path**
from the root to the problematic value:

```cpp
#define JSON_DIAGNOSTICS 1
#include <nlohmann/json.hpp>
```

Error message with diagnostics:
```
[json.exception.type_error.302] (/config/server/port) type must be string, but is number
```

The path is computed by walking up the parent chain (stored in each
`basic_json` node when diagnostics are enabled).

## `parse_error`

Thrown when parsing fails.

```cpp
class parse_error : public exception
{
public:
    const std::size_t byte;   // byte position of the error

    static parse_error create(int id_, const position_t& pos,
                              const std::string& what_arg,
                              BasicJsonType* context);
    static parse_error create(int id_, std::size_t byte_,
                              const std::string& what_arg,
                              BasicJsonType* context);
};
```

### Error IDs

| ID | Condition | Example |
|---|---|---|
| 101 | Unexpected token | `parse("}")` |
| 102 | Invalid `\u` escape | `parse("\"\\u000g\"")` |
| 103 | Invalid surrogate pair | `parse("\"\\uDC00\"")` |
| 104 | Invalid JSON Patch | `patch(json::array({42}))` |
| 105 | JSON Patch missing field | `patch(json::array({{{"op","add"}}}))` |
| 106 | Number overflow | Very large number literal |
| 107 | Invalid JSON Pointer | `json_pointer("no-slash")` |
| 108 | Invalid Unicode code point | Code point > U+10FFFF |
| 109 | Invalid UTF-8 in input | Binary data as string |
| 110 | Binary format marker error | Invalid CBOR/MsgPack byte |
| 112 | BSON parse error | Malformed BSON input |
| 113 | UBJSON parse error | Invalid UBJSON type marker |
| 114 | BJData parse error | Invalid BJData structure |
| 115 | Incomplete binary input | Truncated binary data |

### Catching Parse Errors

```cpp
try {
    json j = json::parse("{invalid}");
} catch (json::parse_error& e) {
    std::cerr << "Parse error: " << e.what() << "\n";
    std::cerr << "Error ID: " << e.id << "\n";
    std::cerr << "Byte position: " << e.byte << "\n";
}
```

### Avoiding Exceptions

```cpp
json j = json::parse("invalid", nullptr, false);
if (j.is_discarded()) {
    // Handle parse failure without exception
}
```

## `type_error`

Thrown when a method is called on a JSON value of the wrong type.

```cpp
class type_error : public exception
{
public:
    static type_error create(int id_, const std::string& what_arg,
                             BasicJsonType* context);
};
```

### Error IDs

| ID | Condition | Example |
|---|---|---|
| 301 | Cannot create from type | `json j = std::complex<double>()` |
| 302 | Type mismatch in get | `json("str").get<int>()` |
| 303 | Type mismatch in get_ref | `json(42).get_ref<std::string&>()` |
| 304 | Wrong type for at() | `json(42).at("key")` |
| 305 | Wrong type for operator[] | `json("str")[0]` |
| 306 | Wrong type for value() | `json(42).value("k", 0)` |
| 307 | Cannot erase from type | `json(42).erase(0)` |
| 308 | Wrong type for push_back | `json("str").push_back(1)` |
| 309 | Wrong type for insert | `json(42).insert(...)` |
| 310 | Wrong type for swap | `json(42).swap(vec)` |
| 311 | Wrong type for iterator | `json(42).begin().key()` |
| 312 | Cannot serialize binary to text | `json::binary(...).dump()` |
| 313 | Wrong type for push_back pair | `json(42).push_back({"k",1})` |
| 314 | unflatten: value not primitive | `json({"/a": [1]}).unflatten()` |
| 315 | unflatten: conflicting paths | `json({"/a": 1, "/a/b": 2}).unflatten()` |
| 316 | Invalid UTF-8 in dump (strict) | Invalid byte in string |
| 317 | to_bson: top level not object | `json::to_bson(json::array())` |
| 318 | to_bson: key too long | Key > max int32 length |

### Common Type Errors

```cpp
json j = 42;

// 302: type mismatch
try {
    std::string s = j.get<std::string>();
} catch (json::type_error& e) {
    // type must be string, but is number
}

// 304: wrong type for at()
try {
    j.at("key");
} catch (json::type_error& e) {
    // cannot use at() with number
}

// 316: invalid UTF-8
try {
    json j = std::string("\xC0\xAF");  // overlong encoding
    j.dump();
} catch (json::type_error& e) {
    // invalid utf-8 byte
}
```

## `out_of_range`

Thrown when accessing elements outside valid bounds.

```cpp
class out_of_range : public exception
{
public:
    static out_of_range create(int id_, const std::string& what_arg,
                               BasicJsonType* context);
};
```

### Error IDs

| ID | Condition | Example |
|---|---|---|
| 401 | Array index out of range | `json({1,2}).at(5)` |
| 402 | Array index `-` in at() | `j.at("/-"_json_pointer)` |
| 403 | Key not found | `j.at("missing")` |
| 404 | JSON Pointer reference error | `j["/bad/path"_json_pointer]` |
| 405 | back()/pop_back() on empty ptr | `json_pointer("").back()` |
| 406 | Numeric overflow in get | Large float → int |
| 407 | Number not representable | `json(1e500).get<int>()` |
| 408 | BSON key conflict | Key "0" in BSON array |

```cpp
json j = {1, 2, 3};

try {
    j.at(10);
} catch (json::out_of_range& e) {
    // [json.exception.out_of_range.401] array index 10 is out of range
}
```

## `invalid_iterator`

Thrown when iterators are used incorrectly.

```cpp
class invalid_iterator : public exception
{
public:
    static invalid_iterator create(int id_, const std::string& what_arg,
                                   BasicJsonType* context);
};
```

### Error IDs

| ID | Condition |
|---|---|
| 201 | Iterator not dereferenceable |
| 202 | Iterator += on non-array |
| 203 | Iterator compare across values |
| 204 | Iterator - on non-array |
| 205 | Iterator > on non-array |
| 206 | Iterator + on non-array |
| 207 | Cannot use key() on array iterator |
| 209 | Range [first, last) not from same container |
| 210 | Range not valid for erase |
| 211 | Range not valid for insert |
| 212 | Range from different container in insert |
| 213 | Insert iterator for non-array |
| 214 | Insert range for non-object |

```cpp
json j1 = {1, 2, 3};
json j2 = {4, 5, 6};

try {
    j1.erase(j2.begin());  // wrong container
} catch (json::invalid_iterator& e) {
    // iterator does not fit current value
}
```

## `other_error`

Thrown for miscellaneous errors that don't fit the other categories.

```cpp
class other_error : public exception
{
public:
    static other_error create(int id_, const std::string& what_arg,
                              BasicJsonType* context);
};
```

### Error IDs

| ID | Condition |
|---|---|
| 501 | JSON Patch test operation failed |

```cpp
json doc = {{"name", "alice"}};
json patch = json::array({
    {{"op", "test"}, {"path", "/name"}, {"value", "bob"}}
});

try {
    doc.patch(patch);
} catch (json::other_error& e) {
    // [json.exception.other_error.501] unsuccessful: /name
}
```

## Exception-Free API

### `parse()` with `allow_exceptions = false`

```cpp
json j = json::parse("invalid", nullptr, false);
if (j.is_discarded()) {
    // Handle gracefully
}
```

### `get()` Alternatives

Use `value()` for safe object access with defaults:

```cpp
json j = {{"timeout", 30}};
int t = j.value("timeout", 60);      // 30
int r = j.value("retries", 3);       // 3 (missing key)
```

Use `contains()` before access:

```cpp
if (j.contains("key")) {
    auto val = j["key"];
}
```

Use `find()` for iterator-based access:

```cpp
auto it = j.find("key");
if (it != j.end()) {
    // use *it
}
```

### Type Checking Before Access

```cpp
json j = /* unknown content */;

if (j.is_string()) {
    auto s = j.get<std::string>();
}
```

## Catching All JSON Exceptions

```cpp
try {
    // JSON operations
} catch (json::exception& e) {
    std::cerr << "JSON error [" << e.id << "]: " << e.what() << "\n";
}
```

Since `json::exception` derives from `std::exception`, it can also be
caught generically:

```cpp
try {
    // ...
} catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
}
```
