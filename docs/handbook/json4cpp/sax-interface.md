# json4cpp — SAX Interface

## Overview

The SAX (Simple API for XML/JSON) interface provides an event-driven
parsing model. Instead of building a complete DOM tree in memory, the
parser reports structural events to a handler as it reads the input.

This is useful for:
- Processing very large JSON documents without loading them fully
- Filtering or transforming data during parsing
- Building custom data structures directly from JSON input
- Reducing memory usage

## `json_sax` Abstract Class

Defined in `include/nlohmann/detail/input/json_sax.hpp`:

```cpp
template<typename BasicJsonType>
struct json_sax
{
    using number_integer_t  = typename BasicJsonType::number_integer_t;
    using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
    using number_float_t    = typename BasicJsonType::number_float_t;
    using string_t          = typename BasicJsonType::string_t;
    using binary_t          = typename BasicJsonType::binary_t;

    virtual bool null() = 0;
    virtual bool boolean(bool val) = 0;
    virtual bool number_integer(number_integer_t val) = 0;
    virtual bool number_unsigned(number_unsigned_t val) = 0;
    virtual bool number_float(number_float_t val, const string_t& s) = 0;
    virtual bool string(string_t& val) = 0;
    virtual bool binary(binary_t& val) = 0;

    virtual bool start_object(std::size_t elements) = 0;
    virtual bool key(string_t& val) = 0;
    virtual bool end_object() = 0;

    virtual bool start_array(std::size_t elements) = 0;
    virtual bool end_array() = 0;

    virtual bool parse_error(std::size_t position,
                             const std::string& last_token,
                             const detail::exception& ex) = 0;

    json_sax() = default;
    json_sax(const json_sax&) = default;
    json_sax(json_sax&&) noexcept = default;
    json_sax& operator=(const json_sax&) = default;
    json_sax& operator=(json_sax&&) noexcept = default;
    virtual ~json_sax() = default;
};
```

## Event Methods

### Scalar Events

| Method | Called When | Arguments |
|---|---|---|
| `null()` | `null` literal parsed | — |
| `boolean(val)` | `true` or `false` parsed | `bool val` |
| `number_integer(val)` | Signed integer parsed | `number_integer_t val` |
| `number_unsigned(val)` | Unsigned integer parsed | `number_unsigned_t val` |
| `number_float(val, s)` | Float parsed | `number_float_t val`, `string_t s` (raw text) |
| `string(val)` | String parsed | `string_t& val` |
| `binary(val)` | Binary data parsed | `binary_t& val` |

### Container Events

| Method | Called When | Arguments |
|---|---|---|
| `start_object(n)` | `{` read | Element count hint (or -1 if unknown) |
| `key(val)` | Object key read | `string_t& val` |
| `end_object()` | `}` read | — |
| `start_array(n)` | `[` read | Element count hint (or -1 if unknown) |
| `end_array()` | `]` read | — |

### Error Event

| Method | Called When | Arguments |
|---|---|---|
| `parse_error(pos, tok, ex)` | Parse error | Byte position, last token, exception |

### Return Values

All methods return `bool`:
- `true` — continue parsing
- `false` — abort parsing immediately

For `parse_error()`:
- `true` — abort with no exception (return discarded value)
- `false` — abort with no exception (return discarded value)

In practice, the return value of `parse_error()` has the same effect
regardless — the parser stops. The distinction matters for whether
exceptions are thrown (controlled by `allow_exceptions`).

## Using `sax_parse()`

```cpp
template<typename InputType, typename SAX>
static bool sax_parse(InputType&& i,
                      SAX* sax,
                      input_format_t format = input_format_t::json,
                      const bool strict = true,
                      const bool ignore_comments = false,
                      const bool ignore_trailing_commas = false);
```

```cpp
MySaxHandler handler;
bool success = json::sax_parse(input_string, &handler);
```

The `format` parameter supports binary formats too:

```cpp
json::sax_parse(cbor_data, &handler, json::input_format_t::cbor);
json::sax_parse(msgpack_data, &handler, json::input_format_t::msgpack);
```

## Implementing a Custom SAX Handler

### Minimal Handler (Count Elements)

```cpp
struct counter_handler : nlohmann::json_sax<json>
{
    std::size_t values = 0;
    std::size_t objects = 0;
    std::size_t arrays = 0;

    bool null() override { ++values; return true; }
    bool boolean(bool) override { ++values; return true; }
    bool number_integer(json::number_integer_t) override { ++values; return true; }
    bool number_unsigned(json::number_unsigned_t) override { ++values; return true; }
    bool number_float(json::number_float_t, const std::string&) override { ++values; return true; }
    bool string(std::string&) override { ++values; return true; }
    bool binary(json::binary_t&) override { ++values; return true; }

    bool start_object(std::size_t) override { ++objects; return true; }
    bool key(std::string&) override { return true; }
    bool end_object() override { return true; }

    bool start_array(std::size_t) override { ++arrays; return true; }
    bool end_array() override { return true; }

    bool parse_error(std::size_t, const std::string&,
                     const nlohmann::detail::exception&) override
    { return false; }
};

// Usage
counter_handler handler;
json::sax_parse(R"({"a": [1, 2], "b": true})", &handler);
// handler.values == 3  (1, 2, true)
// handler.objects == 1
// handler.arrays == 1
```

### Key Extractor

Extract all keys from a JSON document without building the DOM:

```cpp
struct key_extractor : nlohmann::json_sax<json>
{
    std::vector<std::string> keys;
    int depth = 0;

    bool null() override { return true; }
    bool boolean(bool) override { return true; }
    bool number_integer(json::number_integer_t) override { return true; }
    bool number_unsigned(json::number_unsigned_t) override { return true; }
    bool number_float(json::number_float_t, const std::string&) override { return true; }
    bool string(std::string&) override { return true; }
    bool binary(json::binary_t&) override { return true; }

    bool start_object(std::size_t) override { ++depth; return true; }
    bool key(std::string& val) override {
        keys.push_back(val);
        return true;
    }
    bool end_object() override { --depth; return true; }

    bool start_array(std::size_t) override { return true; }
    bool end_array() override { return true; }

    bool parse_error(std::size_t, const std::string&,
                     const nlohmann::detail::exception&) override
    { return false; }
};
```

### Early Termination

Return `false` from any method to stop parsing immediately:

```cpp
struct find_key_handler : nlohmann::json_sax<json>
{
    std::string target_key;
    json found_value;
    bool found = false;
    bool capture_next = false;

    bool key(std::string& val) override {
        capture_next = (val == target_key);
        return true;
    }

    bool string(std::string& val) override {
        if (capture_next) {
            found_value = val;
            found = true;
            return false;  // stop parsing
        }
        return true;
    }

    bool number_integer(json::number_integer_t val) override {
        if (capture_next) {
            found_value = val;
            found = true;
            return false;
        }
        return true;
    }

    // ... remaining methods return true ...
    bool null() override { return !capture_next || (found = true, false); }
    bool boolean(bool v) override {
        if (capture_next) { found_value = v; found = true; return false; }
        return true;
    }
    bool number_unsigned(json::number_unsigned_t v) override {
        if (capture_next) { found_value = v; found = true; return false; }
        return true;
    }
    bool number_float(json::number_float_t v, const std::string&) override {
        if (capture_next) { found_value = v; found = true; return false; }
        return true;
    }
    bool binary(json::binary_t&) override { return true; }
    bool start_object(std::size_t) override { capture_next = false; return true; }
    bool end_object() override { return true; }
    bool start_array(std::size_t) override { capture_next = false; return true; }
    bool end_array() override { return true; }
    bool parse_error(std::size_t, const std::string&,
                     const nlohmann::detail::exception&) override { return false; }
};
```

## Built-in SAX Handlers

### `json_sax_dom_parser`

The default handler used by `parse()`. Builds a complete DOM tree:

```cpp
template<typename BasicJsonType>
class json_sax_dom_parser
{
    BasicJsonType& root;
    std::vector<BasicJsonType*> ref_stack;
    BasicJsonType* object_element = nullptr;
};
```

### `json_sax_dom_callback_parser`

Used when `parse()` is called with a callback. Adds filtering logic:

```cpp
template<typename BasicJsonType>
class json_sax_dom_callback_parser
{
    BasicJsonType& root;
    std::vector<BasicJsonType*> ref_stack;
    std::vector<bool> keep_stack;
    std::vector<bool> key_keep_stack;
    parser_callback_t<BasicJsonType> callback;
};
```

### `json_sax_acceptor`

Used by `accept()`. All event methods return `true`, `parse_error()`
returns `false`:

```cpp
template<typename BasicJsonType>
struct json_sax_acceptor {
    bool null() { return true; }
    bool boolean(bool) { return true; }
    // ... all true ...
    bool parse_error(...) { return false; }
};
```

## SAX with Binary Formats

The SAX interface works uniformly across all supported formats. The
`binary_reader` generates the same SAX events from binary input:

```cpp
struct my_handler : nlohmann::json_sax<json> { /* ... */ };

my_handler handler;

// JSON text
json::sax_parse(json_text, &handler);

// CBOR
json::sax_parse(cbor_bytes, &handler, json::input_format_t::cbor);

// MessagePack
json::sax_parse(msgpack_bytes, &handler, json::input_format_t::msgpack);
```

The `start_object(n)` and `start_array(n)` methods receive the element
count as `n` for binary formats (where the count is known from the header).
For JSON text, `n` is always `static_cast<std::size_t>(-1)` (unknown).

## Performance Considerations

SAX parsing avoids DOM construction overhead:
- No heap allocations for JSON containers
- No recursive destruction of the DOM tree
- Constant memory usage (proportional to nesting depth only)
- Can process arbitrarily large documents

For streaming scenarios where you need to process multiple JSON values
from a single input, use `sax_parse()` with `strict = false` in a loop.
