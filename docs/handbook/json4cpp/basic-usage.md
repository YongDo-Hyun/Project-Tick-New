# json4cpp — Basic Usage

## Including the Library

```cpp
#include <nlohmann/json.hpp>

// Convenience alias
using json = nlohmann::json;
```

Or with the forward declaration header (for header files):

```cpp
#include <nlohmann/json_fwd.hpp>  // declares json, ordered_json, json_pointer
```

## Creating JSON Values

### Null

```cpp
json j;                     // default constructor → null
json j = nullptr;           // explicit null
json j(nullptr);            // explicit null
json j(json::value_t::null); // from value_t enum
```

### Boolean

```cpp
json j = true;
json j = false;
json j(json::value_t::boolean);  // false (default-initialized)
```

### Numbers

```cpp
// Integer (stored as number_integer_t = std::int64_t)
json j = 42;
json j = -100;

// Unsigned (stored as number_unsigned_t = std::uint64_t)
json j = 42u;
json j = static_cast<std::uint64_t>(100);

// Floating-point (stored as number_float_t = double)
json j = 3.14;
json j = 1.0e10;
```

### String

```cpp
json j = "hello world";
json j = std::string("hello");

// With C++17 string_view:
json j = std::string_view("hello");
```

### Array

```cpp
// From initializer list
json j = {1, 2, 3, 4, 5};

// Explicit array factory
json j = json::array();           // empty array
json j = json::array({1, 2, 3}); // pre-populated

// From value_t enum
json j(json::value_t::array);    // empty array

// From count and value
json j(5, "x");  // ["x", "x", "x", "x", "x"]
```

### Object

```cpp
// From initializer list of key-value pairs
json j = {
    {"name", "Alice"},
    {"age", 30},
    {"active", true}
};

// Explicit object factory
json j = json::object();
json j = json::object({{"key", "value"}});

// From value_t enum
json j(json::value_t::object);

// The library auto-detects objects vs arrays in initializer lists:
// All elements are [string, value] pairs → object
// Otherwise → array
json obj = {{"a", 1}, {"b", 2}};  // → object
json arr = {1, 2, 3};              // → array
json arr2 = {{1, 2}, {3, 4}};     // → array of arrays
```

### Binary

```cpp
// Binary data without subtype
json j = json::binary({0x01, 0x02, 0x03});

// Binary data with subtype (used by MessagePack ext, CBOR tags, etc.)
json j = json::binary({0x01, 0x02}, 42);

// From std::vector<std::uint8_t>
std::vector<std::uint8_t> data = {0xCA, 0xFE};
json j = json::binary(data);
json j = json::binary(std::move(data));  // move semantics
```

### From Existing Types

The `basic_json` constructor template accepts any "compatible type" —
any type for which a `to_json()` overload exists:

```cpp
// Standard containers
std::vector<int> v = {1, 2, 3};
json j = v;  // [1, 2, 3]

std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
json j = m;  // {"a": 1, "b": 2}

// Pairs and tuples (C++11)
std::pair<std::string, int> p = {"key", 42};
json j = p;  // ["key", 42]

// Enum types (unless JSON_DISABLE_ENUM_SERIALIZATION is set)
enum Color { Red, Green, Blue };
json j = Green;  // 1
```

## Parsing JSON

### From String

```cpp
// Static parse method
json j = json::parse(R"({"key": "value", "number": 42})");

// From std::string
std::string input = R"([1, 2, 3])";
json j = json::parse(input);

// User-defined literal (requires JSON_USE_GLOBAL_UDLS or using namespace)
auto j = R"({"key": "value"})"_json;
```

### From Stream

```cpp
#include <fstream>

std::ifstream file("data.json");
json j = json::parse(file);

// Or with stream extraction operator:
json j;
file >> j;
```

### From Iterator Pair

```cpp
std::string input = R"({"key": "value"})";
json j = json::parse(input.begin(), input.end());

// Works with any input iterator
std::vector<char> data = ...;
json j = json::parse(data.begin(), data.end());
```

### Parse Options

```cpp
json j = json::parse(
    input,
    nullptr,        // callback (nullptr = no callback)
    true,           // allow_exceptions (true = throw on error)
    false,          // ignore_comments (false = comments are errors)
    false           // ignore_trailing_commas (false = trailing commas are errors)
);
```

### Error Handling During Parsing

```cpp
// Option 1: Exceptions (default)
try {
    json j = json::parse("invalid json");
} catch (json::parse_error& e) {
    std::cerr << e.what() << "\n";
    // [json.exception.parse_error.101] parse error at line 1, column 1:
    //   syntax error while parsing value - invalid literal; ...
}

// Option 2: No exceptions
json j = json::parse("invalid json", nullptr, false);
if (j.is_discarded()) {
    // parsing failed
}
```

### Validation Without Parsing

```cpp
bool valid = json::accept(R"({"key": "value"})");   // true
bool invalid = json::accept("not json");              // false

// With options
bool valid = json::accept(input, true, true);  // ignore comments, trailing commas
```

### Parser Callbacks

Filter or modify values during parsing:

```cpp
json j = json::parse(input, [](int depth, json::parse_event_t event, json& parsed) {
    // event: object_start, object_end, array_start, array_end, key, value
    // Return false to discard the value
    if (event == json::parse_event_t::key && parsed == json("password")) {
        return false;  // discard objects with "password" key
    }
    return true;
});
```

## Serialization

### To String

```cpp
json j = {{"name", "Alice"}, {"age", 30}};

// Compact (no indentation)
std::string s = j.dump();
// {"age":30,"name":"Alice"}

// Pretty-printed (4-space indent)
std::string s = j.dump(4);
// {
//     "age": 30,
//     "name": "Alice"
// }

// Custom indent character
std::string s = j.dump(1, '\t');

// Force ASCII output
std::string s = j.dump(-1, ' ', true);
// Non-ASCII chars are escaped as \uXXXX
```

### `dump()` Method Signature

```cpp
string_t dump(
    const int indent = -1,
    const char indent_char = ' ',
    const bool ensure_ascii = false,
    const error_handler_t error_handler = error_handler_t::strict
) const;
```

The `error_handler` controls how invalid UTF-8 in strings is handled:

| Value | Behavior |
|---|---|
| `error_handler_t::strict` | Throw `type_error::316` |
| `error_handler_t::replace` | Replace invalid bytes with U+FFFD |
| `error_handler_t::ignore` | Skip invalid bytes |

### To Stream

```cpp
std::cout << j << std::endl;             // compact
std::cout << std::setw(4) << j << "\n";  // pretty

// To file
std::ofstream file("output.json");
file << std::setw(4) << j;
```

## Type Inspection

### Type Query Methods

```cpp
json j = 42;

j.type()           // value_t::number_integer
j.type_name()      // "number"

j.is_null()        // false
j.is_boolean()     // false
j.is_number()      // true
j.is_number_integer()   // true
j.is_number_unsigned()  // false
j.is_number_float()     // false
j.is_object()      // false
j.is_array()       // false
j.is_string()      // false
j.is_binary()      // false
j.is_discarded()   // false

j.is_primitive()   // true  (null, string, boolean, number, binary)
j.is_structured()  // false (object or array)
```

### Explicit Type Conversion

```cpp
json j = 42;

// Using get<T>()
int i = j.get<int>();
double d = j.get<double>();
std::string s = j.get<std::string>();  // throws type_error::302

// Using get_to()
int i;
j.get_to(i);

// Using get_ref<T&>()
json j = "hello";
const std::string& ref = j.get_ref<const std::string&>();

// Using get_ptr<T*>()
json j = "hello";
const std::string* ptr = j.get_ptr<const std::string*>();
if (ptr != nullptr) {
    // use *ptr
}
```

### Implicit Type Conversion

When `JSON_USE_IMPLICIT_CONVERSIONS` is enabled (default):

```cpp
json j = 42;
int i = j;               // implicit conversion

json j = "hello";
std::string s = j;       // implicit conversion

json j = {1, 2, 3};
std::vector<int> v = j;  // implicit conversion
```

### Cast to `value_t`

```cpp
json j = 42;
json::value_t t = j;  // implicit cast via operator value_t()
if (t == json::value_t::number_integer) { ... }
```

## Working with Objects

### Creating and Modifying

```cpp
json j;  // null

// operator[] implicitly converts null to object/array
j["name"] = "Alice";  // null → object, then insert
j["age"] = 30;
j["scores"] = {95, 87, 92};

// Nested objects
j["address"]["city"] = "Springfield";
j["address"]["state"] = "IL";
```

### Checking Keys

```cpp
if (j.contains("name")) { ... }
if (j.count("name") > 0) { ... }
if (j.find("name") != j.end()) { ... }
```

### Removing Keys

```cpp
j.erase("name");          // by key
j.erase(j.find("age"));   // by iterator
```

### Getting with Default Value

```cpp
std::string name = j.value("name", "unknown");
int port = j.value("port", 8080);
```

## Working with Arrays

### Creating and Modifying

```cpp
json arr = json::array();
arr.push_back(1);
arr.push_back("hello");
arr += 3.14;              // operator+=

arr.emplace_back("world"); // in-place construction

// Insert at position
arr.insert(arr.begin(), 0);
arr.insert(arr.begin() + 2, {10, 20});
```

### Accessing Elements

```cpp
int first = arr[0];
int second = arr.at(1);    // bounds-checked
int last = arr.back();
int first2 = arr.front();
```

### Modifying

```cpp
arr.erase(arr.begin());      // remove first element
arr.erase(2);                 // remove element at index 2
arr.clear();                   // remove all elements
```

### Size and Capacity

```cpp
arr.size();      // number of elements
arr.empty();     // true if no elements
arr.max_size();  // maximum possible elements
```

## Ordered JSON

For insertion-order preservation:

```cpp
nlohmann::ordered_json j;
j["z"] = 1;
j["a"] = 2;
j["m"] = 3;

// Iteration preserves insertion order: z, a, m
for (auto& [key, val] : j.items()) {
    std::cout << key << ": " << val << "\n";
}
```

The `ordered_json` type uses `nlohmann::ordered_map` (a `std::vector`-based
map) instead of `std::map`. Lookups are O(n) instead of O(log n).

## Copy and Comparison

### Copying

```cpp
json j1 = {{"key", "value"}};
json j2 = j1;        // deep copy
json j3(j1);          // deep copy
json j4 = std::move(j1);  // move (j1 becomes null)
```

### Comparison

```cpp
json a = {1, 2, 3};
json b = {1, 2, 3};

a == b;    // true
a != b;    // false
a < b;     // false (same type, same value)

// Cross-type numeric comparison
json(1) == json(1.0);  // true
json(1) < json(1.5);   // true
```

## Structured Bindings (C++17)

```cpp
json j = {{"name", "Alice"}, {"age", 30}};

for (auto& [key, val] : j.items()) {
    std::cout << key << " = " << val << "\n";
}
```

## Common Patterns

### Configuration File Loading

```cpp
json load_config(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return json::object();
    }
    return json::parse(file, nullptr, true, true);  // allow comments
}

auto config = load_config("config.json");
int port = config.value("port", 8080);
std::string host = config.value("host", "localhost");
```

### Safe Value Extraction

```cpp
template<typename T>
std::optional<T> safe_get(const json& j, const std::string& key) {
    if (j.contains(key)) {
        try {
            return j.at(key).get<T>();
        } catch (const json::type_error&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}
```

### Building JSON Programmatically

```cpp
json build_response(int status, const std::string& message) {
    return {
        {"status", status},
        {"message", message},
        {"timestamp", std::time(nullptr)},
        {"data", json::object()}
    };
}
```

### Merging Objects

```cpp
json defaults = {{"color", "blue"}, {"size", 10}, {"visible", true}};
json user_prefs = {{"color", "red"}, {"opacity", 0.8}};

defaults.update(user_prefs);
// defaults = {"color": "red", "size": 10, "visible": true, "opacity": 0.8}

// Deep merge with merge_objects=true
defaults.update(user_prefs, true);
```

### Flattening and Unflattening

```cpp
json nested = {
    {"a", {{"b", {{"c", 42}}}}}
};

json flat = nested.flatten();
// {"/a/b/c": 42}

json restored = flat.unflatten();
// {"a": {"b": {"c": 42}}}
```

## Error Handling Summary

| Exception | When Thrown |
|---|---|
| `json::parse_error` | Invalid JSON input |
| `json::type_error` | Wrong type access (e.g., `string` on a number) |
| `json::out_of_range` | Index/key not found with `at()` |
| `json::invalid_iterator` | Invalid iterator operation |
| `json::other_error` | Miscellaneous errors |

```cpp
try {
    json j = json::parse("...");
    int val = j.at("missing_key").get<int>();
} catch (json::parse_error& e) {
    // e.id: 101, 102, 103, 104, 105
    // e.byte: position in input
} catch (json::out_of_range& e) {
    // e.id: 401, 402, 403, 404, 405
} catch (json::type_error& e) {
    // e.id: 301, 302, 303, 304, 305, 306, 307, 308, ...
}
```
