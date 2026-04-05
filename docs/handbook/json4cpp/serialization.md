# json4cpp — Serialization & Deserialization

## Parsing (Deserialization)

### `parse()`

```cpp
template<typename InputType>
static basic_json parse(InputType&& i,
                        const parser_callback_t cb = nullptr,
                        const bool allow_exceptions = true,
                        const bool ignore_comments = false,
                        const bool ignore_trailing_commas = false);
```

Parses JSON text from multiple source types.

### Accepted Input Types

| Input Type | Example |
|---|---|
| `std::string` / `string_view` | `json::parse("{}")` |
| `const char*` | `json::parse(ptr)` |
| `std::istream&` | `json::parse(file_stream)` |
| Iterator pair | `json::parse(vec.begin(), vec.end())` |
| `FILE*` | `json::parse(std::fopen("f.json", "r"))` |
| Contiguous container | `json::parse(std::vector<uint8_t>{...})` |

### String Parsing

```cpp
auto j = json::parse(R"({"key": "value", "num": 42})");
```

### File Parsing

```cpp
std::ifstream f("config.json");
json j = json::parse(f);
```

### Iterator Parsing

```cpp
std::string s = R"([1, 2, 3])";
json j = json::parse(s.begin(), s.end());

std::vector<uint8_t> bytes = {'{', '}' };
json j2 = json::parse(bytes);
```

### Error Handling

By default, `parse()` throws `json::parse_error` on invalid input:

```cpp
try {
    json j = json::parse("not json");
} catch (json::parse_error& e) {
    std::cerr << e.what() << "\n";
    // "[json.exception.parse_error.101] parse error at line 1, column 1:
    //  syntax error while parsing value - invalid literal; ..."
    std::cerr << "byte: " << e.byte << "\n";  // position of error
}
```

Set `allow_exceptions = false` to get a discarded value instead:

```cpp
json j = json::parse("not json", nullptr, false);
assert(j.is_discarded());
```

### Comments

JSON does not support comments by standard, but the parser can skip them:

```cpp
std::string input = R"({
    // line comment
    "key": "value",
    /* block comment */
    "num": 42
})";

json j = json::parse(input, nullptr, true, true);  // ignore_comments=true
```

Both C-style (`/* */`) and C++-style (`//`) comments are supported.

### Trailing Commas

```cpp
std::string input = R"({
    "a": 1,
    "b": 2,
})";

json j = json::parse(input, nullptr, true, false, true);  // ignore_trailing_commas=true
```

### `operator>>`

Stream extraction operator:

```cpp
std::istringstream ss(R"({"key": "value"})");
json j;
ss >> j;
```

### `_json` User-Defined Literal

```cpp
using namespace nlohmann::literals;

auto j = R"({"key": "value"})"_json;
auto j2 = "[1, 2, 3]"_json;
```

The UDL is also available via `using namespace nlohmann::json_literals` or
`using namespace nlohmann::literals`. When `JSON_GlobalUDLs` is enabled
(the default), the literals are in the global namespace via
`inline namespace`.

### `_json_pointer` Literal

```cpp
using namespace nlohmann::literals;

auto ptr = "/foo/bar/0"_json_pointer;
```

## Parser Callbacks

### `parse_event_t`

```cpp
enum class parse_event_t : std::uint8_t
{
    object_start,   ///< the parser read `{` and started to process a JSON object
    object_end,     ///< the parser read `}` and finished processing a JSON object
    array_start,    ///< the parser read `[` and started to process a JSON array
    array_end,      ///< the parser read `]` and finished processing a JSON array
    key,            ///< the parser read a key of a value in an object
    value           ///< the parser finished reading a JSON value
};
```

### Callback Signature

```cpp
using parser_callback_t = std::function<bool(int depth,
                                              parse_event_t event,
                                              basic_json& parsed)>;
```

- `depth` — nesting depth (0 = top level)
- `event` — current parse event
- `parsed` — the parsed value (for `value`/`key` events) or null (for start/end)
- Return `true` to keep the value, `false` to discard

### Filtering Example

```cpp
// Remove all keys named "password"
json j = json::parse(input, [](int /*depth*/, json::parse_event_t event, json& parsed) {
    if (event == json::parse_event_t::key && parsed == "password") {
        return false;
    }
    return true;
});
```

### Depth-Limited Parsing

```cpp
// Only keep top-level keys
json j = json::parse(input, [](int depth, json::parse_event_t event, json&) {
    if (depth > 1 && event == json::parse_event_t::value) {
        return false;
    }
    return true;
});
```

## Serialization

### `dump()`

```cpp
string_t dump(const int indent = -1,
              const char indent_char = ' ',
              const bool ensure_ascii = false,
              const error_handler_t error_handler = error_handler_t::strict) const;
```

Converts a JSON value to a string.

### Compact Output

```cpp
json j = {{"name", "alice"}, {"scores", {90, 85, 92}}};
std::string s = j.dump();
// {"name":"alice","scores":[90,85,92]}
```

### Pretty Printing

```cpp
std::string s = j.dump(4);
// {
//     "name": "alice",
//     "scores": [
//         90,
//         85,
//         92
//     ]
// }
```

You can change the indent character:

```cpp
std::string s = j.dump(1, '\t');
// {
// 	"name": "alice",
// 	"scores": [
// 		90,
// 		85,
// 		92
// 	]
// }
```

### ASCII Escaping

When `ensure_ascii = true`, all non-ASCII characters are escaped:

```cpp
json j = "München";
j.dump();              // "München"
j.dump(-1, ' ', true); // "M\u00FCnchen"
```

### UTF-8 Error Handling

The `error_handler` controls what happens when the serializer encounters
invalid UTF-8:

```cpp
enum class error_handler_t
{
    strict,   ///< throw type_error::316 on invalid UTF-8
    replace,  ///< replace invalid bytes with U+FFFD
    ignore    ///< skip invalid bytes silently
};
```

```cpp
// String with invalid UTF-8 byte 0xFF
std::string bad = "hello\xFFworld";
json j = bad;

j.dump();  // throws type_error::316

j.dump(-1, ' ', false, json::error_handler_t::replace);
// "hello\uFFFDworld"

j.dump(-1, ' ', false, json::error_handler_t::ignore);
// "helloworld"
```

### `operator<<`

Stream insertion operator for convenience:

```cpp
json j = {{"key", "value"}};
std::cout << j << "\n";         // compact: {"key":"value"}
std::cout << std::setw(4) << j; // pretty: 4-space indent
```

Using `std::setw()` with the stream sets the indentation level.

## Serializer Internals

The actual serialization is performed by `detail::serializer<basic_json>`
(in `include/nlohmann/detail/output/serializer.hpp`):

```cpp
template<typename BasicJsonType>
class serializer
{
public:
    serializer(output_adapter_t<char> s, const char ichar,
               error_handler_t error_handler_ = error_handler_t::strict);

    void dump(const BasicJsonType& val, const bool pretty_print,
              const bool ensure_ascii, const unsigned int indent_step,
              const unsigned int current_indent = 0);

private:
    void dump_escaped(const string_t& s, const bool ensure_ascii);
    void dump_integer(number_integer_t x);
    void dump_integer(number_unsigned_t x);
    void dump_float(number_float_t x, std::true_type is_ieee);
    void dump_float(number_float_t x, std::false_type is_ieee);
};
```

### Number Serialization

**Integers** are serialized using a custom digit-by-digit algorithm that
writes into a stack buffer (`number_buffer`), avoiding `std::to_string`:

```cpp
// Internal buffer for number conversion
std::array<char, 64> number_buffer{{}};
```

**Floating-point** values use different strategies:
- On IEEE 754 platforms (`std::true_type`): uses `std::snprintf` with
  `%.*g` format, with precision from `std::numeric_limits<number_float_t>::max_digits10`
- On non-IEEE platforms (`std::false_type`): same `snprintf` approach

Special float values:
- `NaN` → serialized as `null`
- `Infinity` → serialized as `null`

### String Escaping

The `dump_escaped()` method handles:
- Control characters (`\n`, `\t`, `\r`, `\b`, `\f`, `\\`, `\"`)
- Characters 0x00–0x1F are escaped as `\u00XX`
- Non-ASCII characters can be escaped as `\uXXXX` (if `ensure_ascii = true`)
- Surrogate pairs for characters above U+FFFF
- Invalid UTF-8 handling per `error_handler_t`

The UTF-8 decoder uses a state machine:

```cpp
static const std::array<std::uint8_t, 400> utf8d;
std::uint8_t decode(std::uint8_t& state, std::uint32_t& codep, const std::uint8_t byte);
```

States: `UTF8_ACCEPT` (0) and `UTF8_REJECT` (1).

## Output Adapters

Defined in `include/nlohmann/detail/output/output_adapters.hpp`:

```cpp
template<typename CharType>
class output_adapter
{
public:
    // Adapts std::vector<CharType>
    output_adapter(std::vector<CharType>& vec);

    // Adapts std::basic_ostream
    output_adapter(std::basic_ostream<CharType>& s);

    // Adapts std::basic_string
    output_adapter(StringType& s);
};
```

Three concrete adapters:
- `output_vector_adapter` — writes to `std::vector<char>`
- `output_stream_adapter` — writes to `std::ostream`
- `output_string_adapter` — writes to `std::string`

## ADL Serialization Mechanism

### How `to_json()` / `from_json()` Work

The library uses **Argument-Dependent Lookup (ADL)** to find conversion
functions. When you call `json j = my_obj;`, the library ultimately invokes:

```cpp
nlohmann::adl_serializer<MyType>::to_json(j, my_obj);
```

The default `adl_serializer` delegates to a free function found via ADL:

```cpp
template<typename ValueType, typename>
struct adl_serializer
{
    template<typename BasicJsonType, typename TargetType = ValueType>
    static auto from_json(BasicJsonType&& j, TargetType& val)
        -> decltype(::nlohmann::from_json(std::forward<BasicJsonType>(j), val), void())
    {
        ::nlohmann::from_json(std::forward<BasicJsonType>(j), val);
    }

    template<typename BasicJsonType, typename TargetType = ValueType>
    static auto to_json(BasicJsonType& j, TargetType&& val)
        -> decltype(::nlohmann::to_json(j, std::forward<TargetType>(val)), void())
    {
        ::nlohmann::to_json(j, std::forward<TargetType>(val));
    }
};
```

### Built-in Conversions

The library provides `to_json()` and `from_json()` overloads for:

| C++ Type | JSON Type |
|---|---|
| `bool` | boolean |
| `int`, `long`, `int64_t`, etc. | number_integer |
| `unsigned`, `uint64_t`, etc. | number_unsigned |
| `float`, `double` | number_float |
| `std::string`, `const char*` | string |
| `std::nullptr_t` | null |
| `std::vector<T>`, `std::list<T>`, ... | array |
| `std::array<T, N>` | array |
| `std::map<string, T>` | object |
| `std::unordered_map<string, T>` | object |
| `std::pair<T1, T2>` | array of 2 |
| `std::tuple<Ts...>` | array of N |
| `std::optional<T>` (C++17) | value or null |
| `std::variant<Ts...>` (C++17) | depends on held type |
| Enum types | integer (unless disabled) |

### Priority Tags for Overload Resolution

Built-in `to_json()` overloads use a priority tag system to resolve
ambiguity:

```cpp
template<typename BasicJsonType, typename T>
void to_json(BasicJsonType& j, T val, priority_tag<1>);  // higher priority
template<typename BasicJsonType, typename T>
void to_json(BasicJsonType& j, T val, priority_tag<0>);  // lower priority
```

The `priority_tag<N>` inherits from `priority_tag<N-1>`, so higher-tagged
overloads are preferred.

## Roundtrip Guarantees

### Integers

Integer values survive a parse → dump → parse roundtrip exactly, as long
as they fit within `int64_t` or `uint64_t` range.

### Floating-Point

The library uses `max_digits10` precision (typically 17 for `double`) to
ensure roundtrip fidelity:

```cpp
json j = 3.141592653589793;
std::string s = j.dump();     // "3.141592653589793"
json j2 = json::parse(s);
assert(j == j2);              // true
```

### Strings

UTF-8 strings roundtrip exactly. The serializer preserves all valid UTF-8
sequences. Unicode escapes in input (`\uXXXX`) are converted to UTF-8 on
parsing and will be re-escaped only if `ensure_ascii = true`.

## `accept()`

```cpp
template<typename InputType>
static bool accept(InputType&& i,
                   const bool ignore_comments = false,
                   const bool ignore_trailing_commas = false);
```

Checks whether the input is valid JSON without constructing a value:

```cpp
json::accept("{}");           // true
json::accept("[1,2,3]");      // true
json::accept("not json");     // false
json::accept("{\"a\": 1,}");  // false
json::accept("{\"a\": 1,}", false, true);  // true (trailing commas)
```

## Conversion to/from STL Types

### Explicit Conversion (`get<T>()`)

```cpp
json j = 42;
int i = j.get<int>();
double d = j.get<double>();

json j2 = {1, 2, 3};
auto v = j2.get<std::vector<int>>();
auto s = j2.get<std::set<int>>();
```

### Implicit Conversion

When `JSON_ImplicitConversions` is enabled (default), implicit conversions
are available:

```cpp
json j = 42;
int i = j;            // implicit conversion
std::string s = j;    // throws type_error::302 (wrong type)

json j2 = "hello";
std::string s2 = j2;  // "hello"
```

To disable implicit conversions (recommended for new code):

```cmake
set(JSON_ImplicitConversions OFF)
```

Or define the macro:

```cpp
#define JSON_USE_IMPLICIT_CONVERSIONS 0
```

When disabled, only explicit `get<T>()` works.
