# json4cpp — Parsing Internals

## Parser Architecture

The parsing pipeline consists of three stages:

```
Input → InputAdapter → Lexer → Parser → JSON value
                                  ↓
                           SAX Handler
```

1. **Input adapters** normalize various input sources into a uniform byte stream
2. **Lexer** tokenizes the byte stream into JSON tokens
3. **Parser** implements a recursive descent parser driven by SAX events

## Input Adapters

Defined in `include/nlohmann/detail/input/input_adapters.hpp`.

### Adapter Hierarchy

```cpp
// File input
class file_input_adapter {
    std::FILE* m_file;
    std::char_traits<char>::int_type get_character();
};

// Stream input
class input_stream_adapter {
    std::istream* is;
    std::streambuf* sb;
    std::char_traits<char>::int_type get_character();
};

// Iterator-based input
template<typename IteratorType>
class iterator_input_adapter {
    IteratorType current;
    IteratorType end;
    std::char_traits<char>::int_type get_character();
};
```

All adapters expose a `get_character()` method that returns the next byte
or `std::char_traits<char>::eof()` at end of input.

### `input_adapter()` Factory

The free function `input_adapter()` selects the appropriate adapter:

```cpp
// From string/string_view
auto adapter = input_adapter(std::string("{}"));

// From iterators
auto adapter = input_adapter(vec.begin(), vec.end());

// From stream
auto adapter = input_adapter(std::cin);
```

### Span Input Adapter

For contiguous memory (C++17):

```cpp
template<typename CharT>
class contiguous_bytes_input_adapter {
    const CharT* current;
    const CharT* end;
};
```

This is the fastest adapter since it reads directly from memory without
virtual dispatch.

## Lexer

Defined in `include/nlohmann/detail/input/lexer.hpp`. The lexer
(scanner/tokenizer) converts a byte stream into a sequence of tokens.

### Token Types

```cpp
enum class token_type
{
    uninitialized,       ///< indicating the scanner is uninitialized
    literal_true,        ///< the 'true' literal
    literal_false,       ///< the 'false' literal
    literal_null,        ///< the 'null' literal
    value_string,        ///< a string (includes the quotes)
    value_unsigned,      ///< an unsigned integer
    value_integer,       ///< a signed integer
    value_float,         ///< a floating-point number
    begin_array,         ///< the character '['
    begin_object,        ///< the character '{'
    end_array,           ///< the character ']'
    end_object,          ///< the character '}'
    name_separator,      ///< the character ':'
    value_separator,     ///< the character ','
    parse_error,         ///< indicating a parse error
    end_of_input         ///< indicating the end of the input buffer
};
```

### Lexer Class

```cpp
template<typename BasicJsonType, typename InputAdapterType>
class lexer : public lexer_base<BasicJsonType>
{
public:
    using number_integer_t  = typename BasicJsonType::number_integer_t;
    using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
    using number_float_t    = typename BasicJsonType::number_float_t;
    using string_t          = typename BasicJsonType::string_t;

    // Main scanning entry point
    token_type scan();

    // Access scanned values
    constexpr number_integer_t  get_number_integer()  const noexcept;
    constexpr number_unsigned_t get_number_unsigned() const noexcept;
    constexpr number_float_t    get_number_float()    const noexcept;
    string_t& get_string();

    // Error information
    constexpr position_t get_position() const noexcept;
    std::string get_token_string() const;
    const std::string& get_error_message() const noexcept;

private:
    InputAdapterType ia;           // input source
    char_int_type current;         // current character
    bool next_unget = false;       // lookahead flag
    position_t position {};        // line/column tracking
    std::vector<char_type> token_string {};  // raw token for error messages
    string_t token_buffer {};      // decoded string value
    // Number storage (only one is valid at a time)
    number_integer_t  value_integer  = 0;
    number_unsigned_t value_unsigned = 0;
    number_float_t    value_float    = 0;
};
```

### Position Tracking

```cpp
struct position_t
{
    std::size_t chars_read_total = 0;       // total characters read
    std::size_t chars_read_current_line = 0; // characters on current line
    std::size_t lines_read = 0;              // lines read (newline count)
};
```

### String Scanning

The `scan_string()` method handles:
- Regular characters
- Escape sequences: `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`
- Unicode escapes: `\uXXXX` (including surrogate pairs for `\uD800`–`\uDBFF` + `\uDC00`–`\uDFFF`)
- UTF-8 validation using a state machine

### Number Scanning

The `scan_number()` method determines the number type:

1. Parse sign (optional `-`)
2. Parse integer part
3. If `.` follows → parse fractional part → `value_float`
4. If `e`/`E` follows → parse exponent → `value_float`
5. Otherwise, try to fit into `number_integer_t` or `number_unsigned_t`

The method first accumulates the raw characters, then converts:
- Integers: `std::strtoull` / `std::strtoll`
- Floats: `std::strtod`

### Comment Scanning

When `ignore_comments` is enabled:

```cpp
bool scan_comment() {
    // After seeing '/', check next char:
    // '/' → scan to end of line (C++ comment)
    // '*' → scan to '*/' (C comment)
}
```

## Parser

Defined in `include/nlohmann/detail/input/parser.hpp`. Implements a
**recursive descent** parser that generates SAX events.

### Parser Class

```cpp
template<typename BasicJsonType, typename InputAdapterType>
class parser
{
public:
    using number_integer_t  = typename BasicJsonType::number_integer_t;
    using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
    using number_float_t    = typename BasicJsonType::number_float_t;
    using string_t          = typename BasicJsonType::string_t;
    using lexer_t           = lexer<BasicJsonType, InputAdapterType>;

    parser(InputAdapterType&& adapter,
           const parser_callback_t<BasicJsonType> cb = nullptr,
           const bool allow_exceptions_ = true,
           const bool skip_comments = false,
           const bool ignore_trailing_commas_ = false);

    void parse(const bool strict, BasicJsonType& result);
    bool accept(const bool strict = true);

    template<typename SAX>
    bool sax_parse(SAX* sax, const bool strict = true);

private:
    template<typename SAX>
    bool sax_parse_internal(SAX* sax);

    lexer_t m_lexer;
    token_type last_token = token_type::uninitialized;
    bool allow_exceptions;
    bool ignore_trailing_commas;
};
```

### Recursive Descent Grammar

The parser implements the JSON grammar:

```
json      → value
value     → object | array | string | number | "true" | "false" | "null"
object    → '{' (pair (',' pair)* ','?)? '}'
pair      → string ':' value
array     → '[' (value (',' value)* ','?)? ']'
```

The trailing comma handling is optional (controlled by
`ignore_trailing_commas`).

### SAX-Driven Parsing

The parser calls SAX handler methods as it encounters JSON structure:

```cpp
template<typename SAX>
bool sax_parse_internal(SAX* sax)
{
    switch (last_token) {
        case token_type::begin_object:
            // 1. sax->start_object(...)
            // 2. For each key-value:
            //    a. sax->key(string)
            //    b. recurse into sax_parse_internal for value
            // 3. sax->end_object()
            break;
        case token_type::begin_array:
            // 1. sax->start_array(...)
            // 2. For each element: recurse into sax_parse_internal
            // 3. sax->end_array()
            break;
        case token_type::value_string:
            return sax->string(m_lexer.get_string());
        case token_type::value_unsigned:
            return sax->number_unsigned(m_lexer.get_number_unsigned());
        case token_type::value_integer:
            return sax->number_integer(m_lexer.get_number_integer());
        case token_type::value_float:
            // Check for NaN: store as null
            return sax->number_float(m_lexer.get_number_float(), ...);
        case token_type::literal_true:
            return sax->boolean(true);
        case token_type::literal_false:
            return sax->boolean(false);
        case token_type::literal_null:
            return sax->null();
        default:
            return sax->parse_error(...);
    }
}
```

### DOM Construction

Two SAX handlers build the DOM tree:

#### `json_sax_dom_parser`

Standard DOM builder. Each SAX event creates or appends to the JSON tree:

```cpp
template<typename BasicJsonType>
class json_sax_dom_parser
{
    BasicJsonType& root;
    std::vector<BasicJsonType*> ref_stack;  // stack of parent nodes
    BasicJsonType* object_element = nullptr;
    bool errored = false;
    bool allow_exceptions;

    bool null();
    bool boolean(bool val);
    bool number_integer(number_integer_t val);
    bool number_unsigned(number_unsigned_t val);
    bool number_float(number_float_t val, const string_t& s);
    bool string(string_t& val);
    bool binary(binary_t& val);
    bool start_object(std::size_t elements);
    bool end_object();
    bool start_array(std::size_t elements);
    bool end_array();
    bool key(string_t& val);
    bool parse_error(std::size_t position, const std::string& last_token,
                     const detail::exception& ex);
};
```

The `ref_stack` tracks the current nesting path. On `start_object()` /
`start_array()`, a new container is pushed. On `end_object()` /
`end_array()`, the stack is popped.

#### `json_sax_dom_callback_parser`

Extends the DOM builder with callback support. When the callback returns
`false`, the value is discarded:

```cpp
template<typename BasicJsonType>
class json_sax_dom_callback_parser
{
    BasicJsonType& root;
    std::vector<BasicJsonType*> ref_stack;
    std::vector<bool> keep_stack;   // tracks which values to keep
    std::vector<bool> key_keep_stack;
    BasicJsonType* object_element = nullptr;
    BasicJsonType discarded = BasicJsonType::value_t::discarded;
    parser_callback_t<BasicJsonType> callback;
    bool errored = false;
    bool allow_exceptions;
};
```

## `accept()` Method

The `accept()` method checks validity without building a DOM:

```cpp
bool accept(const bool strict = true);
```

Internally it uses `json_sax_acceptor` — a SAX handler where all methods
return `true` (accepting everything) and `parse_error()` returns `false`:

```cpp
template<typename BasicJsonType>
struct json_sax_acceptor
{
    bool null() { return true; }
    bool boolean(bool) { return true; }
    bool number_integer(number_integer_t) { return true; }
    // ... all return true ...
    bool parse_error(...) { return false; }
};
```

## `sax_parse()` — Static SAX Entry Point

```cpp
template<typename InputType, typename SAX>
static bool sax_parse(InputType&& i, SAX* sax,
                      input_format_t format = input_format_t::json,
                      const bool strict = true,
                      const bool ignore_comments = false,
                      const bool ignore_trailing_commas = false);
```

The `input_format_t` enum selects the parser:

```cpp
enum class input_format_t {
    json,
    cbor,
    msgpack,
    ubjson,
    bson,
    bjdata
};
```

For `json`, the text parser is used. For binary formats, the
`binary_reader` is used (which also generates SAX events).

## Error Reporting

### Parse Error Format

```
[json.exception.parse_error.101] parse error at line 3, column 5:
syntax error while parsing object key - unexpected end of input;
expected string literal
```

The error message includes:
- Exception ID (e.g., 101)
- Position (line and column, or byte offset)
- Description of what was expected vs. what was found
- The last token read (for context)

### Error IDs

| ID | Condition |
|---|---|
| 101 | Unexpected token |
| 102 | `\u` escape with invalid hex digits |
| 103 | Invalid UTF-8 surrogate pair |
| 104 | JSON Patch: invalid patch document |
| 105 | JSON Patch: missing required field |
| 106 | Invalid number format |
| 107 | Invalid JSON Pointer syntax |
| 108 | Invalid Unicode code point |
| 109 | Invalid UTF-8 byte sequence |
| 110 | Unrecognized CBOR/MessagePack/UBJSON/BSON marker |
| 112 | Parse error in BSON |
| 113 | Parse error in UBJSON |
| 114 | Parse error in BJData |
| 115 | Parse error due to incomplete binary data |

### Diagnostic Positions

When `JSON_DIAGNOSTIC_POSITIONS` is enabled at compile time, the library
tracks byte positions for each value. Error messages then include
`start_position` and `end_position` for the offending value:

```cpp
#define JSON_DIAGNOSTICS 1
#define JSON_DIAGNOSTIC_POSITIONS 1
```

## Parser Callback Events

The parser callback receives events defined by `parse_event_t`:

```cpp
enum class parse_event_t : std::uint8_t
{
    object_start,   // '{' read
    object_end,     // '}' read
    array_start,    // '[' read
    array_end,      // ']' read
    key,            // object key read
    value           // value read
};
```

Callback invocation points in the parser:
1. `object_start` — after `{` is consumed, before any key
2. `key` — after a key string is consumed, `parsed` = the key string
3. `value` — after any value is fully parsed, `parsed` = the value
4. `object_end` — after `}` is consumed, `parsed` = the complete object
5. `array_start` — after `[` is consumed, before any element
6. `array_end` — after `]` is consumed, `parsed` = the complete array

### Callback Return Value

- `true` → keep the value
- `false` → discard (replace with `discarded`)

For container events (`object_start`, `array_start`), returning `false`
skips the **entire** container and all its contents.

## Performance Characteristics

| Stage | Complexity | Dominant Cost |
|---|---|---|
| Input adapter | O(n) | Single pass over input |
| Lexer | O(n) | Character-by-character scan, string copy |
| Parser | O(n) | Recursive descent, SAX event dispatch |
| DOM construction | O(n) | Memory allocation for containers |

The overall parsing complexity is O(n) in the input size. Memory usage is
proportional to the nesting depth (parser stack) plus the size of the
resulting DOM (heap allocations for strings, arrays, objects).

For large inputs where the full DOM is not needed, using the SAX interface
directly avoids DOM construction overhead entirely.
