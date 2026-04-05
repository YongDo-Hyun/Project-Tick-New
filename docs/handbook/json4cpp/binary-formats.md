# json4cpp — Binary Formats

## Overview

The library supports five binary serialization formats in addition to JSON
text. All are available as static methods on `basic_json`:

| Format | To | From | RFC/Spec |
|---|---|---|---|
| CBOR | `to_cbor()` | `from_cbor()` | RFC 7049 |
| MessagePack | `to_msgpack()` | `from_msgpack()` | MessagePack spec |
| UBJSON | `to_ubjson()` | `from_ubjson()` | UBJSON spec |
| BSON | `to_bson()` | `from_bson()` | BSON spec |
| BJData | `to_bjdata()` | `from_bjdata()` | BJData spec |

Binary serialization is useful for:
- Smaller payload sizes
- Faster parsing
- Native binary data support
- Type-rich encodings (timestamps, binary subtypes)

## CBOR (Concise Binary Object Representation)

### Serialization

```cpp
// To vector<uint8_t>
static std::vector<std::uint8_t> to_cbor(const basic_json& j);

// To output adapter (stream, string, vector)
static void to_cbor(const basic_json& j, detail::output_adapter<uint8_t> o);
static void to_cbor(const basic_json& j, detail::output_adapter<char> o);
```

```cpp
json j = {{"compact", true}, {"schema", 0}};

// Serialize to byte vector
auto cbor = json::to_cbor(j);

// Serialize to stream
std::ofstream out("data.cbor", std::ios::binary);
json::to_cbor(j, out);
```

### Deserialization

```cpp
template<typename InputType>
static basic_json from_cbor(InputType&& i,
                            const bool strict = true,
                            const bool allow_exceptions = true,
                            const cbor_tag_handler_t tag_handler = cbor_tag_handler_t::error);
```

Parameters:
- `strict` — if `true`, requires that all bytes are consumed
- `allow_exceptions` — if `false`, returns discarded value on error
- `tag_handler` — how to handle CBOR tags

```cpp
auto j = json::from_cbor(cbor);
```

### CBOR Tag Handling

```cpp
enum class cbor_tag_handler_t
{
    error,   ///< throw parse_error on any tag
    ignore,  ///< ignore tags
    store    ///< store tags as binary subtype
};
```

```cpp
// Ignore CBOR tags
auto j = json::from_cbor(data, true, true, json::cbor_tag_handler_t::ignore);

// Store CBOR tags as subtypes in binary values
auto j = json::from_cbor(data, true, true, json::cbor_tag_handler_t::store);
```

### CBOR Type Mapping

| JSON Type | CBOR Type |
|---|---|
| null | null (0xF6) |
| boolean | true/false (0xF5/0xF4) |
| number_integer | negative/unsigned integer |
| number_unsigned | unsigned integer |
| number_float | IEEE 754 double (0xFB) or half-precision (0xF9) |
| string | text string (major type 3) |
| array | array (major type 4) |
| object | map (major type 5) |
| binary | byte string (major type 2) |

## MessagePack

### Serialization

```cpp
static std::vector<std::uint8_t> to_msgpack(const basic_json& j);
static void to_msgpack(const basic_json& j, detail::output_adapter<uint8_t> o);
static void to_msgpack(const basic_json& j, detail::output_adapter<char> o);
```

```cpp
json j = {{"array", {1, 2, 3}}, {"null", nullptr}};
auto msgpack = json::to_msgpack(j);
```

### Deserialization

```cpp
template<typename InputType>
static basic_json from_msgpack(InputType&& i,
                               const bool strict = true,
                               const bool allow_exceptions = true);
```

```cpp
auto j = json::from_msgpack(msgpack);
```

### MessagePack Type Mapping

| JSON Type | MessagePack Type |
|---|---|
| null | nil (0xC0) |
| boolean | true/false (0xC3/0xC2) |
| number_integer | int 8/16/32/64 or negative fixint |
| number_unsigned | uint 8/16/32/64 or positive fixint |
| number_float | float 32 or float 64 |
| string | fixstr / str 8/16/32 |
| array | fixarray / array 16/32 |
| object | fixmap / map 16/32 |
| binary | bin 8/16/32 |
| binary with subtype | ext 8/16/32 / fixext 1/2/4/8/16 |

The library chooses the **smallest** encoding that fits the value.

### Ext Types

MessagePack extension types carry a type byte. The library maps this to the
binary subtype:

```cpp
json j = json::binary({0x01, 0x02, 0x03}, 42);  // subtype 42
auto mp = json::to_msgpack(j);
// Encoded as ext with type byte 42

auto j2 = json::from_msgpack(mp);
assert(j2.get_binary().subtype() == 42);
```

## UBJSON (Universal Binary JSON)

### Serialization

```cpp
static std::vector<std::uint8_t> to_ubjson(const basic_json& j,
                                            const bool use_size = false,
                                            const bool use_type = false);
```

Parameters:
- `use_size` — write container size markers (enables optimized containers)
- `use_type` — write type markers for homogeneous containers (requires `use_size`)

```cpp
json j = {1, 2, 3, 4, 5};

// Without optimization
auto ub1 = json::to_ubjson(j);

// With size optimization
auto ub2 = json::to_ubjson(j, true);

// With size+type optimization (smallest for homogeneous arrays)
auto ub3 = json::to_ubjson(j, true, true);
```

### Deserialization

```cpp
template<typename InputType>
static basic_json from_ubjson(InputType&& i,
                              const bool strict = true,
                              const bool allow_exceptions = true);
```

### UBJSON Type Markers

| Marker | Type |
|---|---|
| `Z` | null |
| `T` / `F` | true / false |
| `i` | int8 |
| `U` | uint8 |
| `I` | int16 |
| `l` | int32 |
| `L` | int64 |
| `d` | float32 |
| `D` | float64 |
| `C` | char |
| `S` | string |
| `[` / `]` | array begin / end |
| `{` / `}` | object begin / end |
| `H` | high-precision number (string representation) |

## BSON (Binary JSON)

### Serialization

```cpp
static std::vector<std::uint8_t> to_bson(const basic_json& j);
static void to_bson(const basic_json& j, detail::output_adapter<uint8_t> o);
static void to_bson(const basic_json& j, detail::output_adapter<char> o);
```

**Important:** BSON requires the top-level value to be an **object**:

```cpp
json j = {{"key", "value"}, {"num", 42}};
auto bson = json::to_bson(j);

// json j = {1, 2, 3};
// json::to_bson(j);  // throws type_error::317 — not an object
```

### Deserialization

```cpp
template<typename InputType>
static basic_json from_bson(InputType&& i,
                            const bool strict = true,
                            const bool allow_exceptions = true);
```

### BSON Type Mapping

| JSON Type | BSON Type |
|---|---|
| null | 0x0A (Null) |
| boolean | 0x08 (Boolean) |
| number_integer | 0x10 (int32) or 0x12 (int64) |
| number_unsigned | 0x10 or 0x12 (depends on value) |
| number_float | 0x01 (double) |
| string | 0x02 (String) |
| array | 0x04 (Array) — encoded as object with "0", "1", ... keys |
| object | 0x03 (Document) |
| binary | 0x05 (Binary) |

### BSON Binary Subtypes

```cpp
json j;
j["data"] = json::binary({0x01, 0x02}, 0x80);  // subtype 0x80
auto bson = json::to_bson(j);
// Binary encoded with subtype byte 0x80
```

## BJData (Binary JData)

BJData extends UBJSON with additional types for N-dimensional arrays and
optimized integer types.

### Serialization

```cpp
static std::vector<std::uint8_t> to_bjdata(const basic_json& j,
                                            const bool use_size = false,
                                            const bool use_type = false);
```

### Deserialization

```cpp
template<typename InputType>
static basic_json from_bjdata(InputType&& i,
                              const bool strict = true,
                              const bool allow_exceptions = true);
```

### Additional BJData Types

Beyond UBJSON types, BJData adds:

| Marker | Type |
|---|---|
| `u` | uint16 |
| `m` | uint32 |
| `M` | uint64 |
| `h` | float16 (half-precision) |

## Roundtrip Between Formats

Binary formats can preserve the same data as JSON text, but with some
differences:

```cpp
json original = {
    {"name", "test"},
    {"values", {1, 2, 3}},
    {"data", json::binary({0xFF, 0xFE})}
};

// JSON text cannot represent binary
std::string text = original.dump();
// "data" field would cause issues in text form

// Binary formats can represent binary natively
auto cbor = json::to_cbor(original);
auto restored = json::from_cbor(cbor);
assert(original == restored);  // exact roundtrip

// Cross-format conversion
auto mp = json::to_msgpack(original);
auto from_mp = json::from_msgpack(mp);
assert(original == from_mp);
```

## Size Comparison

Typical size savings over JSON text:

| Data | JSON | CBOR | MessagePack | UBJSON | BSON |
|---|---|---|---|---|---|
| `{"a":1}` | 7 bytes | 5 bytes | 4 bytes | 6 bytes | 18 bytes |
| `[1,2,3]` | 7 bytes | 4 bytes | 4 bytes | 6 bytes | N/A (top-level array) |
| `true` | 4 bytes | 1 byte | 1 byte | 1 byte | N/A (top-level bool) |

BSON has the most overhead due to its document-structure requirements.
MessagePack and CBOR are generally the most compact.

## Stream-Based Serialization

All binary formats support streaming to/from `std::ostream` / `std::istream`:

```cpp
// Write CBOR to file
std::ofstream out("data.cbor", std::ios::binary);
json::to_cbor(j, out);

// Read CBOR from file
std::ifstream in("data.cbor", std::ios::binary);
json j = json::from_cbor(in);
```

## Strict vs. Non-Strict Parsing

All `from_*` functions accept a `strict` parameter:

- `strict = true` (default): all input bytes must be consumed. Extra
  trailing data causes a parse error.
- `strict = false`: parsing stops after the first valid value. Remaining
  input is ignored.

```cpp
std::vector<uint8_t> data = /* two CBOR values concatenated */;

// Strict: fails because of trailing data
// json::from_cbor(data, true);

// Non-strict: parses only the first value
json j = json::from_cbor(data, false);
```

## Binary Reader / Writer Architecture

The binary I/O is implemented by two internal classes:

### `binary_reader`

Located in `include/nlohmann/detail/input/binary_reader.hpp`:

```cpp
template<typename BasicJsonType, typename InputAdapterType, typename SAX>
class binary_reader
{
    bool parse_cbor_internal(bool get_char, int tag_handler);
    bool parse_msgpack_internal();
    bool parse_ubjson_internal(bool get_char = true);
    bool parse_bson_internal();
    bool parse_bjdata_internal();
    // ...
};
```

Uses the SAX interface internally — each decoded value is reported to a
SAX handler (typically `json_sax_dom_parser`) which builds the JSON tree.

### `binary_writer`

Located in `include/nlohmann/detail/output/binary_writer.hpp`:

```cpp
template<typename BasicJsonType, typename CharType>
class binary_writer
{
    void write_cbor(const BasicJsonType& j);
    void write_msgpack(const BasicJsonType& j);
    void write_ubjson(const BasicJsonType& j, ...);
    void write_bson(const BasicJsonType& j);
    void write_bjdata(const BasicJsonType& j, ...);
    // ...
};
```

Directly writes encoded bytes to an `output_adapter`.
