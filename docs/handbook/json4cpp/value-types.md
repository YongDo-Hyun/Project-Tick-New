# json4cpp — Value Types

## The `value_t` Enumeration

Defined in `include/nlohmann/detail/value_t.hpp`, the `value_t` enumeration
identifies the type of a `basic_json` value:

```cpp
enum class value_t : std::uint8_t
{
    null,             ///< null value
    object,           ///< unordered set of name/value pairs
    array,            ///< ordered collection of values
    string,           ///< string value
    boolean,          ///< boolean value
    number_integer,   ///< signed integer number
    number_unsigned,  ///< unsigned integer number
    number_float,     ///< floating-point number
    binary,           ///< binary array (ordered collection of bytes)
    discarded         ///< discarded by the parser callback function
};
```

The underlying type is `std::uint8_t` (1 byte), stored in `m_data.m_type`.

## Type-to-Storage Mapping

| `value_t` | C++ Type Alias | Default C++ Type | Storage in `json_value` |
|---|---|---|---|
| `null` | — | — | No active member (pointer set to `nullptr`) |
| `object` | `object_t` | `std::map<std::string, basic_json>` | `object_t* object` |
| `array` | `array_t` | `std::vector<basic_json>` | `array_t* array` |
| `string` | `string_t` | `std::string` | `string_t* string` |
| `boolean` | `boolean_t` | `bool` | `boolean_t boolean` |
| `number_integer` | `number_integer_t` | `std::int64_t` | `number_integer_t number_integer` |
| `number_unsigned` | `number_unsigned_t` | `std::uint64_t` | `number_unsigned_t number_unsigned` |
| `number_float` | `number_float_t` | `double` | `number_float_t number_float` |
| `binary` | `binary_t` | `byte_container_with_subtype<vector<uint8_t>>` | `binary_t* binary` |
| `discarded` | — | — | No storage (used only during parse callback filtering) |

Variable-length types (object, array, string, binary) are stored as **heap-
allocated pointers** to keep the `json_value` union at 8 bytes on 64-bit.

## Type Inspection Methods

### `type()`

Returns the `value_t` of the stored value:

```cpp
constexpr value_t type() const noexcept;
```

```cpp
json j = 42;
assert(j.type() == json::value_t::number_integer);

json j2 = "hello";
assert(j2.type() == json::value_t::string);
```

### `type_name()`

Returns a human-readable string for the current type:

```cpp
const char* type_name() const noexcept;
```

| `value_t` | Returned String |
|---|---|
| `null` | `"null"` |
| `object` | `"object"` |
| `array` | `"array"` |
| `string` | `"string"` |
| `boolean` | `"boolean"` |
| `binary` | `"binary"` |
| `number_integer` | `"number"` |
| `number_unsigned` | `"number"` |
| `number_float` | `"number"` |
| `discarded` | `"discarded"` |

Note that all three numeric types return `"number"`.

```cpp
json j = {1, 2, 3};
std::cout << j.type_name();  // "array"
```

### `is_*()` Methods

All return `constexpr bool` and are `noexcept`:

```cpp
constexpr bool is_null() const noexcept;
constexpr bool is_boolean() const noexcept;
constexpr bool is_number() const noexcept;
constexpr bool is_number_integer() const noexcept;
constexpr bool is_number_unsigned() const noexcept;
constexpr bool is_number_float() const noexcept;
constexpr bool is_object() const noexcept;
constexpr bool is_array() const noexcept;
constexpr bool is_string() const noexcept;
constexpr bool is_binary() const noexcept;
constexpr bool is_discarded() const noexcept;
constexpr bool is_primitive() const noexcept;
constexpr bool is_structured() const noexcept;
```

### Category Methods

```cpp
// is_primitive() == is_null() || is_string() || is_boolean() || is_number() || is_binary()
// is_structured() == is_array() || is_object()
// is_number() == is_number_integer() || is_number_float()
// is_number_integer() == (type == number_integer || type == number_unsigned)
```

Important: `is_number_integer()` returns `true` for **both** signed and unsigned
integers. Use `is_number_unsigned()` to distinguish.

```cpp
json j = 42u;
j.is_number()          // true
j.is_number_integer()  // true
j.is_number_unsigned() // true

json j2 = -5;
j2.is_number_integer()  // true
j2.is_number_unsigned() // false
```

### `operator value_t()`

Implicit conversion to `value_t`:

```cpp
constexpr operator value_t() const noexcept;
```

```cpp
json j = "hello";
json::value_t t = j;  // value_t::string
```

## Null Type

Null is the default value:

```cpp
json j;                          // null
json j = nullptr;                // null
json j(json::value_t::null);     // null

j.is_null()    // true
j.type_name()  // "null"
```

Null values have special behavior:
- `size()` returns 0
- `empty()` returns `true`
- `operator[]` with a string key converts null to an object
- `operator[]` with a numeric index converts null to an array
- `push_back()` converts null to an array

## Object Type

### Internal Representation

```cpp
using object_t = ObjectType<StringType, basic_json,
                            default_object_comparator_t,
                            AllocatorType<std::pair<const StringType, basic_json>>>;
```

Default: `std::map<std::string, basic_json, std::less<>, std::allocator<...>>`

With C++14 transparent comparators, heterogeneous lookup is supported.

### `ordered_json` Objects

When using `ordered_json = basic_json<nlohmann::ordered_map>`, objects
preserve insertion order:

```cpp
nlohmann::ordered_json j;
j["z"] = 1;
j["a"] = 2;
j["m"] = 3;
// iteration order: z, a, m
```

The `ordered_map` uses linear search (O(n) lookup) instead of tree-based
(O(log n)).

## Array Type

### Internal Representation

```cpp
using array_t = ArrayType<basic_json, AllocatorType<basic_json>>;
```

Default: `std::vector<basic_json, std::allocator<basic_json>>`

Arrays can contain mixed types (heterogeneous):

```cpp
json j = {1, "two", 3.0, true, nullptr, {{"nested", "object"}}};
```

## String Type

### Internal Representation

```cpp
using string_t = StringType;  // default: std::string
```

Strings in JSON are Unicode (UTF-8). The library validates UTF-8 during
serialization but stores raw bytes. The `dump()` method's `error_handler`
parameter controls what happens with invalid UTF-8:

- `error_handler_t::strict` — throw `type_error::316`
- `error_handler_t::replace` — replace with U+FFFD
- `error_handler_t::ignore` — skip invalid bytes

## Boolean Type

```cpp
using boolean_t = BooleanType;  // default: bool
```

Stored directly in the union (no heap allocation):

```cpp
json j = true;
bool b = j.get<bool>();
```

## Number Types

### Three Distinct Types

The library distinguishes three numeric types:

```cpp
using number_integer_t  = NumberIntegerType;   // default: std::int64_t
using number_unsigned_t = NumberUnsignedType;   // default: std::uint64_t
using number_float_t    = NumberFloatType;       // default: double
```

During parsing, the lexer determines the best-fit type:
1. If the number has a decimal point or exponent → `number_float`
2. If it fits in `int64_t` → `number_integer`
3. If it fits in `uint64_t` → `number_unsigned`
4. Otherwise → `number_float` (as approximation)

### Cross-Type Comparisons

Numbers of different types are compared correctly:

```cpp
json(1) == json(1.0)     // true
json(1) == json(1u)      // true
json(-1) < json(0u)      // true (signed < unsigned via cast)
```

The comparison logic in `JSON_IMPLEMENT_OPERATOR` handles all 6 cross-type
combinations (int×float, float×int, unsigned×float, float×unsigned,
unsigned×int, int×unsigned).

### NaN Handling

`NaN` values result in unordered comparisons:

```cpp
json nan_val = std::numeric_limits<double>::quiet_NaN();
nan_val == nan_val;  // false (IEEE 754 semantics)
```

## Binary Type

### Internal Representation

```cpp
using binary_t = nlohmann::byte_container_with_subtype<BinaryType>;
// BinaryType default: std::vector<std::uint8_t>
```

`byte_container_with_subtype<BinaryType>` extends `BinaryType` with an
optional subtype tag:

```cpp
template<typename BinaryType>
class byte_container_with_subtype : public BinaryType
{
public:
    using container_type = BinaryType;
    using subtype_type = std::uint64_t;

    void set_subtype(subtype_type subtype_) noexcept;
    constexpr subtype_type subtype() const noexcept;
    constexpr bool has_subtype() const noexcept;
    void clear_subtype() noexcept;

private:
    subtype_type m_subtype = 0;
    bool m_has_subtype = false;
};
```

### Creating Binary Values

```cpp
// Without subtype
json j = json::binary({0x01, 0x02, 0x03, 0x04});

// With subtype
json j = json::binary({0x01, 0x02}, 128);

// Access the binary container
json::binary_t& bin = j.get_binary();
bin.push_back(0x05);
assert(bin.has_subtype() == false);  // depends on how it was created
```

### Subtype Significance

The subtype is meaningful for binary formats:
- **MessagePack**: maps to ext type
- **CBOR**: maps to tag
- **BSON**: maps to binary subtype

JSON text format does not support binary data natively.

## Discarded Type

The `discarded` type is special — it's used only during parsing with
callbacks to indicate that a value should be excluded from the result:

```cpp
json j = json::parse(input, [](int depth, json::parse_event_t event, json& parsed) {
    if (event == json::parse_event_t::key && parsed == "secret") {
        return false;  // discard this key-value pair
    }
    return true;
});
```

When `json::parse()` is called with `allow_exceptions=false` and parsing
fails, the result is a discarded value:

```cpp
json j = json::parse("invalid", nullptr, false);
assert(j.is_discarded());
```

## Type Ordering

The `value_t` enumeration has a defined ordering used for cross-type
comparisons when values can't be compared directly:

```
null < boolean < number < object < array < string < binary
```

This means:

```cpp
json(nullptr) < json(false);     // true (null < boolean)
json(42) < json::object();       // true (number < object)
json("abc") > json::array();     // true (string > array)
```

The ordering is implemented via a lookup array in `value_t.hpp`:

```cpp
static constexpr std::array<std::uint8_t, 9> order = {{
    0 /* null */, 3 /* object */, 4 /* array */, 5 /* string */,
    1 /* boolean */, 2 /* integer */, 2 /* unsigned */, 2 /* float */,
    6 /* binary */
}};
```

All three numeric types share order index 2.

## Type Conversions

### Widening Conversions

```cpp
json j = 42;                   // number_integer
double d = j.get<double>();    // OK: int → double

json j2 = 3.14;               // number_float
int i = j2.get<int>();         // OK: truncates to 3
```

### Container Conversions

```cpp
json arr = {1, 2, 3};
auto v = arr.get<std::vector<int>>();
auto l = arr.get<std::list<int>>();
auto s = arr.get<std::set<int>>();

json obj = {{"a", 1}, {"b", 2}};
auto m = obj.get<std::map<std::string, int>>();
auto um = obj.get<std::unordered_map<std::string, int>>();
```

### String Conversions

```cpp
json j = 42;
std::string s = j.dump();  // "42" (serialization, not type conversion)
// j.get<std::string>()    // throws type_error::302

json j = "hello";
std::string s = j.get<std::string>();  // "hello"
```

## Constructing from `value_t`

You can construct an empty value of a specific type:

```cpp
json j_null(json::value_t::null);        // null
json j_obj(json::value_t::object);       // {}
json j_arr(json::value_t::array);        // []
json j_str(json::value_t::string);       // ""
json j_bool(json::value_t::boolean);     // false
json j_int(json::value_t::number_integer);   // 0
json j_uint(json::value_t::number_unsigned); // 0
json j_float(json::value_t::number_float);   // 0.0
json j_bin(json::value_t::binary);       // binary([], no subtype)
```

## Pointer Access

Low-level pointer access to the underlying value:

```cpp
json j = "hello";

// Returns nullptr if type doesn't match
const std::string* sp = j.get_ptr<const std::string*>();
assert(sp != nullptr);

const int* ip = j.get_ptr<const int*>();
assert(ip == nullptr);  // wrong type

// Mutable pointer access
std::string* sp = j.get_ptr<std::string*>();
*sp = "world";
assert(j == "world");
```

## Reference Access

```cpp
json j = "hello";

const std::string& ref = j.get_ref<const std::string&>();
assert(ref == "hello");

// Throws type_error::303 if type doesn't match
try {
    const int& ref = j.get_ref<const int&>();
} catch (json::type_error& e) {
    // "incompatible ReferenceType for get_ref, actual type is string"
}
```
