# json4cpp — Performance

## Memory Layout

### `json_value` Union

The core storage is a union of 8 members:

```cpp
union json_value
{
    object_t*         object;          // 8 bytes (pointer)
    array_t*          array;           // 8 bytes (pointer)
    string_t*         string;          // 8 bytes (pointer)
    binary_t*         binary;          // 8 bytes (pointer)
    boolean_t         boolean;         // 1 byte
    number_integer_t  number_integer;  // 8 bytes
    number_unsigned_t number_unsigned; // 8 bytes
    number_float_t    number_float;    // 8 bytes
};
```

The union is **8 bytes** on 64-bit platforms. Variable-length types
(object, array, string, binary) are stored as heap-allocated pointers to
keep the union small.

### Total `basic_json` Size

Each `basic_json` node contains:

```cpp
struct data
{
    value_t m_type = value_t::null;  // 1 byte (uint8_t enum)
    // + padding
    json_value m_value = {};          // 8 bytes
};
```

With alignment: **16 bytes per node** on most 64-bit platforms (1 byte
type + 7 bytes padding + 8 bytes value).

When `JSON_DIAGNOSTICS` is enabled, each node additionally stores a parent
pointer:

```cpp
struct data
{
    value_t m_type;
    json_value m_value;
    const basic_json* m_parent = nullptr;  // 8 bytes extra
};
```

Total with diagnostics: **24 bytes per node**.

## Allocation Strategy

### Object Storage (default `std::map`)

- Red-black tree nodes: ~48–64 bytes each (key + value + pointers + color)
- O(log n) lookup, insert, erase
- Good cache locality within individual nodes, poor across the tree

### Array Storage (`std::vector`)

- Contiguous memory: amortized O(1) push_back
- Reallocations: capacity doubles, causing copies of all elements
- Each element is 16 bytes (`basic_json`)

### String Storage (`std::string`)

- SSO (Small String Optimization): strings ≤ ~15 chars stored inline
  (no allocation). Exact threshold is implementation-defined.
- Longer strings: heap allocation

## `ordered_map` Performance

`ordered_json` uses `ordered_map<std::string, basic_json>` which inherits
from `std::vector<std::pair<const Key, T>>`:

| Operation | `std::map` (json) | `ordered_map` (ordered_json) |
|---|---|---|
| Lookup by key | O(log n) | O(n) linear search |
| Insert | O(log n) | O(1) amortized (push_back) |
| Erase by key | O(log n) | O(n) (shift elements) |
| Iteration | O(n), sorted order | O(n), insertion order |
| Memory | Tree nodes (fragmented) | Contiguous vector |

Use `ordered_json` only when insertion order matters and the number of
keys is small (< ~100).

## Destruction

### Iterative Destruction

Deeply nested JSON values would cause stack overflow with recursive
destructors. The library uses **iterative destruction**:

```cpp
void data::destroy(value_t t)
{
    if (t == value_t::array || t == value_t::object)
    {
        // Move children to a flat list
        std::vector<basic_json> stack;
        if (t == value_t::array) {
            stack.reserve(m_value.array->size());
            std::move(m_value.array->begin(), m_value.array->end(),
                      std::back_inserter(stack));
        } else {
            // Extract values from object pairs
            for (auto& pair : *m_value.object) {
                stack.push_back(std::move(pair.second));
            }
        }
        // Continue flattening until stack is empty
        while (!stack.empty()) {
            // Pop and flatten nested containers
        }
    }
    // Destroy the container itself
}
```

This ensures O(1) stack depth regardless of JSON nesting depth.

### Destruction Cost

- Primitives (null, boolean, number): O(1), no heap deallocation
- String: O(1), single `delete`
- Array: O(n), iterative flattening + deallocation of each element
- Object: O(n), iterative flattening + deallocation of each key-value
- Binary: O(1), single `delete`

## Parsing Performance

### Lexer Optimizations

- Single-character lookahead (no backtracking)
- Token string is accumulated in a pre-allocated buffer
- Number parsing avoids `std::string` intermediate: raw chars → integer or
  float directly via `strtoull`/`strtod`
- UTF-8 validation uses a compact state machine (400-byte lookup table)

### Parser Complexity

- O(n) in input size
- O(d) stack depth where d = maximum nesting depth
- SAX approach avoids intermediate DOM allocations

### Fastest Parsing Path

For maximum speed:
1. Use contiguous input (`std::string`, `const char*`, `std::vector<uint8_t>`)
   — avoids virtual dispatch in input adapter
2. Disable comments (`ignore_comments = false`)
3. Disable trailing commas (`ignore_trailing_commas = false`)
4. No callback (`cb = nullptr`)
5. Allow exceptions (`allow_exceptions = true`) — avoids extra bookkeeping

## Serialization Performance

### Number Formatting

- **Integers**: Custom digit-by-digit algorithm writing to a 64-byte stack
  buffer. Faster than `std::to_string` (no `std::string` allocation).
- **Floats**: `std::snprintf` with `max_digits10` precision. The format
  string is `%.*g`.

### String Escaping

- ASCII-only strings: nearly zero overhead (copy + quote wrapping)
- Strings with special characters: per-byte check against escape table
- `ensure_ascii`: full UTF-8 decode + `\uXXXX` encoding (slower)

### Output Adapter

- `output_string_adapter` (default for `dump()`): writes to `std::string`
  with `push_back()` / `append()`
- `output_stream_adapter`: writes to `std::ostream` via `put()` / `write()`
- `output_vector_adapter`: writes to `std::vector<char>` via `push_back()`

## Compilation Time

Being header-only, json.hpp can add significant compilation time. Strategies:

### Single Include vs. Multi-Header

| Approach | Files | Compilation Model |
|---|---|---|
| `single_include/nlohmann/json.hpp` | 1 file (~25K lines) | Include everywhere |
| `include/nlohmann/json.hpp` | Many small headers | Better incremental builds |

### Reducing Compilation Time

1. **Precompiled headers**: Add `nlohmann/json.hpp` to your PCH
2. **Forward declarations**: Use `nlohmann/json_fwd.hpp` in headers, full
   include only in `.cpp` files
3. **Extern template**: Pre-instantiate in one TU:

```cpp
// json_instantiation.cpp
#include <nlohmann/json.hpp>
template class nlohmann::basic_json<>;  // explicit instantiation
```

4. **Minimize includes**: Only include where actually needed

## Binary Format Performance

Size and speed characteristics compared to JSON text:

| Aspect | JSON Text | CBOR | MessagePack | UBJSON |
|---|---|---|---|---|
| Encoding speed | Fast | Fast | Fast | Moderate |
| Decoding speed | Moderate | Fast | Fast | Moderate |
| Output size | Largest | Compact | Most compact | Moderate |
| Human readable | Yes | No | No | No |

Binary formats are generally faster to parse because:
- No string-to-number conversion (numbers stored in binary)
- Size-prefixed containers (no scanning for delimiters)
- No whitespace handling
- No string escape processing

## Best Practices

### Avoid Copies

```cpp
// Bad: copies the entire array
json arr = j["data"];

// Good: reference
const auto& arr = j["data"];
```

### Use `get_ref()` for String Access

```cpp
// Bad: copies the string
std::string s = j.get<std::string>();

// Good: reference (no copy)
const auto& s = j.get_ref<const std::string&>();
```

### Reserve Capacity

```cpp
json j = json::array();
// If you know the size, reserve first (via underlying container)
// The API doesn't expose reserve() directly, but you can:
json j = json::parse(input);  // parser pre-allocates when size hints are available
```

### SAX for Large Documents

```cpp
// Bad: loads entire 1GB file into DOM
json j = json::parse(huge_file);

// Good: process streaming with SAX
struct my_handler : nlohmann::json_sax<json> { /* ... */ };
my_handler handler;
json::sax_parse(huge_file, &handler);
```

### Move Semantics

```cpp
json source = get_data();
json dest = std::move(source);  // O(1) move, source becomes null
```
