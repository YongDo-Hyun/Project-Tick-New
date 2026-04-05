# libnbt++ Architecture

## High-Level Design

libnbt++ follows a classic object-oriented design with a polymorphic tag hierarchy, augmented by C++ template metaprogramming for code reuse. The architecture has five major layers:

1. **Tag Hierarchy** — Polymorphic class tree rooted at `tag`, with concrete types for each NBT tag
2. **Value Layer** — Type-erased wrappers (`value`, `value_initializer`) for runtime tag manipulation
3. **I/O Layer** — Stream-based readers/writers handling binary serialization and endianness
4. **Compression Layer** — zlib stream adapters for transparent gzip/deflate support
5. **Text Layer** — Formatters for human-readable tag output (JSON-like)

---

## Class Hierarchy

```
tag  (abstract base, tag.h)
└── detail::crtp_tag<Sub>  (CRTP intermediate, crtp_tag.h)
    ├── tag_primitive<int8_t>    → typedef tag_byte
    ├── tag_primitive<int16_t>   → typedef tag_short
    ├── tag_primitive<int32_t>   → typedef tag_int
    ├── tag_primitive<int64_t>   → typedef tag_long
    ├── tag_primitive<float>     → typedef tag_float
    ├── tag_primitive<double>    → typedef tag_double
    ├── tag_string
    ├── tag_array<int8_t>        → typedef tag_byte_array
    ├── tag_array<int32_t>       → typedef tag_int_array
    ├── tag_array<int64_t>       → typedef tag_long_array
    ├── tag_list
    └── tag_compound
```

All concrete tag classes are declared `final` — they cannot be further subclassed. The hierarchy uses exactly two levels of inheritance: `tag` → `crtp_tag<Sub>` → concrete class.

---

## The CRTP Pattern

The Curiously Recurring Template Pattern (CRTP) is central to libnbt++'s design. The intermediate class `detail::crtp_tag<Sub>` (defined in `include/crtp_tag.h`) automatically implements all the `tag` virtual methods that can be expressed generically:

```cpp
namespace nbt {
namespace detail {

template <class Sub> class crtp_tag : public tag
{
public:
    virtual ~crtp_tag() noexcept = 0;  // Pure virtual to keep it abstract

    tag_type get_type() const noexcept override final {
        return Sub::type;  // Each Sub has a static constexpr tag_type type
    };

    std::unique_ptr<tag> clone() const& override final {
        return make_unique<Sub>(sub_this());  // Copy-constructs Sub
    }

    std::unique_ptr<tag> move_clone() && override final {
        return make_unique<Sub>(std::move(sub_this()));  // Move-constructs Sub
    }

    tag& assign(tag&& rhs) override final {
        return sub_this() = dynamic_cast<Sub&&>(rhs);
        // Throws std::bad_cast if rhs is not the same Sub type
    }

    void accept(nbt_visitor& visitor) override final {
        visitor.visit(sub_this());  // Double dispatch
    }

    void accept(const_nbt_visitor& visitor) const override final {
        visitor.visit(sub_this());
    }

private:
    bool equals(const tag& rhs) const override final {
        return sub_this() == static_cast<const Sub&>(rhs);
    }

    Sub& sub_this() { return static_cast<Sub&>(*this); }
    const Sub& sub_this() const { return static_cast<const Sub&>(*this); }
};

template <class Sub> crtp_tag<Sub>::~crtp_tag() noexcept {}

} // namespace detail
} // namespace nbt
```

### What the CRTP Provides

Each concrete tag class inherits from `crtp_tag<Self>` and automatically gets:

| Method            | Behavior                                                |
|------------------|---------------------------------------------------------|
| `get_type()`     | Returns `Sub::type` (the static `tag_type` constant)     |
| `clone()`        | Copy-constructs a new `Sub` via `make_unique<Sub>`       |
| `move_clone()`   | Move-constructs a new `Sub`                              |
| `assign(tag&&)`  | Dynamic casts to `Sub&&` and uses `Sub::operator=`       |
| `accept()`       | Calls `visitor.visit(sub_this())` — double dispatch      |
| `equals()`       | Uses `Sub::operator==`                                   |

The concrete class only needs to provide:

1. A `static constexpr tag_type type` member
2. Copy and move constructors/assignment operators
3. `operator==` and `operator!=`
4. `read_payload(io::stream_reader&)` and `write_payload(io::stream_writer&) const`

---

## The tag Base Class

The `tag` base class (defined in `include/tag.h`) establishes the interface for all NBT tags:

```cpp
class NBT_EXPORT tag
{
public:
    virtual ~tag() noexcept {}

    virtual tag_type get_type() const noexcept = 0;

    virtual std::unique_ptr<tag> clone() const& = 0;
    virtual std::unique_ptr<tag> move_clone() && = 0;
    std::unique_ptr<tag> clone() &&;  // Delegates to move_clone

    template <class T> T& as();
    template <class T> const T& as() const;

    virtual tag& assign(tag&& rhs) = 0;

    virtual void accept(nbt_visitor& visitor) = 0;
    virtual void accept(const_nbt_visitor& visitor) const = 0;

    virtual void read_payload(io::stream_reader& reader) = 0;
    virtual void write_payload(io::stream_writer& writer) const = 0;

    static std::unique_ptr<tag> create(tag_type type);
    static std::unique_ptr<tag> create(tag_type type, int8_t val);
    static std::unique_ptr<tag> create(tag_type type, int16_t val);
    static std::unique_ptr<tag> create(tag_type type, int32_t val);
    static std::unique_ptr<tag> create(tag_type type, int64_t val);
    static std::unique_ptr<tag> create(tag_type type, float val);
    static std::unique_ptr<tag> create(tag_type type, double val);

    friend NBT_EXPORT bool operator==(const tag& lhs, const tag& rhs);
    friend NBT_EXPORT bool operator!=(const tag& lhs, const tag& rhs);

private:
    virtual bool equals(const tag& rhs) const = 0;
};
```

### Key Design Choices

1. **`clone()` is ref-qualified**: `const&` for copy-cloning, `&&` for move-cloning. The rvalue `clone()` delegates to `move_clone()`.

2. **`as<T>()` uses `dynamic_cast`**: Provides safe downcasting with `std::bad_cast` on failure.

3. **`operator==` uses RTTI**: The free `operator==` first checks `typeid(lhs) == typeid(rhs)`, then delegates to the virtual `equals()` method.

4. **Factory methods**: `tag::create()` constructs tags by `tag_type` at runtime, supporting both default construction and numeric initialization.

---

## Ownership Model

libnbt++ uses a strict ownership model based on `std::unique_ptr<tag>`:

### Where Ownership Lives

- **`value`** — Owns a single tag via `std::unique_ptr<tag> tag_`
- **`tag_compound`** — Owns values in a `std::map<std::string, value>`
- **`tag_list`** — Owns values in a `std::vector<value>`

### Ownership Rules

1. **Single owner**: Every tag has exactly one owner. No shared ownership.
2. **Deep copying**: `clone()` performs a full deep copy of the entire tag tree.
3. **Move semantics**: Tags can be efficiently moved between owners without copying.
4. **No raw pointers for ownership**: The library never uses raw `new`/`delete` for tag management.

### The value Class

The `value` class (`include/value.h`) is the primary type-erasure mechanism. It wraps `std::unique_ptr<tag>` and provides:

```cpp
class NBT_EXPORT value
{
public:
    value() noexcept {}                               // Empty/null value
    explicit value(std::unique_ptr<tag>&& t) noexcept; // Takes ownership
    explicit value(tag&& t);                           // Clones the tag

    // Move only (no implicit copy)
    value(value&&) noexcept = default;
    value& operator=(value&&) noexcept = default;

    // Explicit copy
    explicit value(const value& rhs);
    value& operator=(const value& rhs);

    // Type conversion
    operator tag&();
    operator const tag&() const;
    tag& get();
    const tag& get() const;
    template <class T> T& as();
    template <class T> const T& as() const;

    // Numeric assignments (existing tag gets updated, or new one created)
    value& operator=(int8_t val);
    value& operator=(int16_t val);
    value& operator=(int32_t val);
    value& operator=(int64_t val);
    value& operator=(float val);
    value& operator=(double val);

    // String assignment
    value& operator=(const std::string& str);
    value& operator=(std::string&& str);

    // Numeric conversions (widening only)
    explicit operator int8_t() const;
    explicit operator int16_t() const;
    explicit operator int32_t() const;
    explicit operator int64_t() const;
    explicit operator float() const;
    explicit operator double() const;
    explicit operator const std::string&() const;

    // Compound access delegation
    value& at(const std::string& key);
    value& operator[](const std::string& key);
    value& operator[](const char* key);

    // List access delegation
    value& at(size_t i);
    value& operator[](size_t i);

    // Null check
    explicit operator bool() const { return tag_ != nullptr; }

    // Direct pointer access
    std::unique_ptr<tag>& get_ptr();
    void set_ptr(std::unique_ptr<tag>&& t);
    tag_type get_type() const;

private:
    std::unique_ptr<tag> tag_;
};
```

### The value_initializer Class

`value_initializer` (`include/value_initializer.h`) extends `value` with **implicit** constructors. It is used as a parameter type in functions like `tag_compound::put()` and `tag_list::push_back()`:

```cpp
class NBT_EXPORT value_initializer : public value
{
public:
    value_initializer(std::unique_ptr<tag>&& t) noexcept;
    value_initializer(std::nullptr_t) noexcept;
    value_initializer(value&& val) noexcept;
    value_initializer(tag&& t);

    value_initializer(int8_t val);    // Creates tag_byte
    value_initializer(int16_t val);   // Creates tag_short
    value_initializer(int32_t val);   // Creates tag_int
    value_initializer(int64_t val);   // Creates tag_long
    value_initializer(float val);     // Creates tag_float
    value_initializer(double val);    // Creates tag_double
    value_initializer(const std::string& str);  // Creates tag_string
    value_initializer(std::string&& str);       // Creates tag_string
    value_initializer(const char* str);         // Creates tag_string
};
```

This is why you can write `compound.put("key", 42)` — the `42` (an `int`) implicitly converts to `value_initializer(int32_t(42))`, which constructs a `tag_int(42)` inside a `value`.

### Why value vs value_initializer?

The separation exists because implicit conversions on `value` itself would cause ambiguity problems. For example, if `value` had an implicit constructor from `tag&&`, then expressions involving compound assignment could be ambiguous. By limiting implicit conversions to `value_initializer` (used only as function parameters), the library avoids these issues.

---

## Template Design

### tag_primitive<T>

Six NBT types share the same structure: a single numeric value. The `tag_primitive<T>` template (`include/tag_primitive.h`) handles all of them:

```cpp
template <class T>
class tag_primitive final : public detail::crtp_tag<tag_primitive<T>>
{
public:
    typedef T value_type;
    static constexpr tag_type type = detail::get_primitive_type<T>::value;

    constexpr tag_primitive(T val = 0) noexcept : value(val) {}

    operator T&();
    constexpr operator T() const;
    constexpr T get() const { return value; }

    tag_primitive& operator=(T val);
    void set(T val);

    void read_payload(io::stream_reader& reader) override;
    void write_payload(io::stream_writer& writer) const override;

private:
    T value;
};
```

The type mapping uses `detail::get_primitive_type<T>` (`include/primitive_detail.h`):

```cpp
template <> struct get_primitive_type<int8_t>  : std::integral_constant<tag_type, tag_type::Byte>   {};
template <> struct get_primitive_type<int16_t> : std::integral_constant<tag_type, tag_type::Short>  {};
template <> struct get_primitive_type<int32_t> : std::integral_constant<tag_type, tag_type::Int>    {};
template <> struct get_primitive_type<int64_t> : std::integral_constant<tag_type, tag_type::Long>   {};
template <> struct get_primitive_type<float>   : std::integral_constant<tag_type, tag_type::Float>  {};
template <> struct get_primitive_type<double>  : std::integral_constant<tag_type, tag_type::Double> {};
```

**Explicit instantiation**: Template instantiations are declared `extern template class NBT_EXPORT tag_primitive<...>` in the header and explicitly instantiated in `src/tag.cpp`. This prevents duplicate template instantiations across translation units.

### tag_array<T>

Three NBT array types share the same vector-based structure. The `tag_array<T>` template (`include/tag_array.h`) handles all of them:

```cpp
template <class T>
class tag_array final : public detail::crtp_tag<tag_array<T>>
{
public:
    typedef typename std::vector<T>::iterator iterator;
    typedef typename std::vector<T>::const_iterator const_iterator;
    typedef T value_type;
    static constexpr tag_type type = detail::get_array_type<T>::value;

    tag_array() {}
    tag_array(std::initializer_list<T> init);
    tag_array(std::vector<T>&& vec) noexcept;

    std::vector<T>& get();
    T& at(size_t i);
    T& operator[](size_t i);
    void push_back(T val);
    void pop_back();
    size_t size() const;
    void clear();

    iterator begin(); iterator end();
    const_iterator begin() const; const_iterator end() const;

    void read_payload(io::stream_reader& reader) override;
    void write_payload(io::stream_writer& writer) const override;

private:
    std::vector<T> data;
};
```

The type mapping uses `detail::get_array_type<T>`:

```cpp
template <> struct get_array_type<int8_t>  : std::integral_constant<tag_type, tag_type::Byte_Array> {};
template <> struct get_array_type<int32_t> : std::integral_constant<tag_type, tag_type::Int_Array>  {};
template <> struct get_array_type<int64_t> : std::integral_constant<tag_type, tag_type::Long_Array> {};
```

**Specialized I/O**: `read_payload` and `write_payload` have explicit template specializations for `int8_t` (byte arrays can be read/written as raw byte blocks) and `int64_t` (long arrays read element-by-element with `read_num`). The generic template handles `int32_t` arrays.

---

## File Roles Breakdown

### Core Headers

| File | Role |
|------|------|
| `include/tag.h` | Defines the `tag` abstract base class, the `tag_type` enum (End through Long_Array, plus Null), `is_valid_type()`, the `create()` factory methods, `operator==`/`!=`, and `operator<<`. Also forward-declares `nbt_visitor`, `const_nbt_visitor`, `io::stream_reader`, and `io::stream_writer`. |
| `include/tagfwd.h` | Forward declarations only. Declares `tag`, `tag_primitive<T>` with all six typedefs, `tag_string`, `tag_array<T>` with all three typedefs, `tag_list`, and `tag_compound`. Used by headers that need type names without full definitions. |
| `include/nbt_tags.h` | Convenience umbrella header. Simply includes `tag_primitive.h`, `tag_string.h`, `tag_array.h`, `tag_list.h`, and `tag_compound.h`. |
| `include/crtp_tag.h` | Defines `detail::crtp_tag<Sub>`, the CRTP intermediate class. Includes `tag.h`, `nbt_visitor.h`, and `make_unique.h`. |
| `include/primitive_detail.h` | Defines `detail::get_primitive_type<T>`, mapping C++ types to `tag_type` values. Uses `std::integral_constant` for compile-time constants. |
| `include/make_unique.h` | Provides `nbt::make_unique<T>(args...)`, a C++11 polyfill for `std::make_unique` (which was only added in C++14). |

### Tag Implementation Headers

| File | Role |
|------|------|
| `include/tag_primitive.h` | Full definition of `tag_primitive<T>` including inline `read_payload`/`write_payload`. The six typedefs (`tag_byte` through `tag_double`) are declared here, along with `extern template` declarations for link-time optimization. |
| `include/tag_string.h` | Full definition of `tag_string`. Wraps `std::string` with constructors from `const std::string&`, `std::string&&`, and `const char*`. Provides implicit conversion operators to `std::string&` and `const std::string&`. |
| `include/tag_array.h` | Full definition of `tag_array<T>` with specialized `read_payload`/`write_payload` for `int8_t`, `int64_t`, and the generic case. The three typedefs (`tag_byte_array`, `tag_int_array`, `tag_long_array`) are at the bottom. |
| `include/tag_list.h` | Full definition of `tag_list`. Stores `std::vector<value>` with a tracked `el_type_` (element type). Provides `of<T>()` static factory, `push_back(value_initializer&&)`, `emplace_back<T, Args...>()`, `set()`, iterators, and I/O methods. |
| `include/tag_compound.h` | Full definition of `tag_compound`. Stores `std::map<std::string, value>`. Provides `at()`, `operator[]`, `put()`, `insert()`, `emplace<T>()`, `erase()`, `has_key()`, iterators, and I/O methods. |

### Value Layer

| File | Role |
|------|------|
| `include/value.h` | Type-erased `value` class wrapping `std::unique_ptr<tag>`. Provides numeric/string assignment operators, conversion operators (with widening semantics), compound/list access delegation via `operator[]`. |
| `include/value_initializer.h` | `value_initializer` subclass of `value` with implicit constructors from primitive types, strings, tags, and `nullptr`. Used as function parameter type. |

### I/O Headers

| File | Role |
|------|------|
| `include/endian_str.h` | The `endian` namespace. Declares `read_little`/`read_big`/`write_little`/`write_big` overloads for all integer and floating-point types. Template functions `read()`/`write()` dispatch based on an `endian::endian` enum. |
| `include/io/stream_reader.h` | `io::stream_reader` class and `io::input_error` exception. Free functions `read_compound()` and `read_tag()`. The reader tracks nesting depth (max 1024) to prevent stack overflow attacks. |
| `include/io/stream_writer.h` | `io::stream_writer` class. Free function `write_tag()`. Defines `max_string_len` (UINT16_MAX) and `max_array_len` (INT32_MAX) constants. |

### Compression Headers

| File | Role |
|------|------|
| `include/io/zlib_streambuf.h` | Base class `zlib::zlib_streambuf` extending `std::streambuf`. Contains input/output buffers (`std::vector<char>`) and a `z_stream` struct. Also defines `zlib::zlib_error` exception. |
| `include/io/izlibstream.h` | `zlib::inflate_streambuf` and `zlib::izlibstream`. Decompresses data read from a wrapped `std::istream`. Auto-detects gzip vs zlib format via `window_bits = 32 + 15`. |
| `include/io/ozlibstream.h` | `zlib::deflate_streambuf` and `zlib::ozlibstream`. Compresses data written to a wrapped `std::ostream`. Supports configurable compression level and gzip vs zlib output. |

### Text Headers

| File | Role |
|------|------|
| `include/text/json_formatter.h` | `text::json_formatter` class with a single `print()` method. |

### Visitor

| File | Role |
|------|------|
| `include/nbt_visitor.h` | `nbt_visitor` and `const_nbt_visitor` abstract base classes with 12 `visit()` overloads each (one per concrete tag type). All overloads have default empty implementations, allowing visitors to override only the types they care about. |

---

## Source File Roles

### Core Sources

| File | Role |
|------|------|
| `src/tag.cpp` | Contains the explicit template instantiation definitions for all six `tag_primitive<T>` specializations. Implements `tag::create()` factory methods (both default and numeric), `operator==`/`!=` (using `typeid` comparison), `operator<<` (delegating to `json_formatter`), and the `tag_type` output operator. Also contains a `static_assert` ensuring IEEE 754 floating point. |
| `src/tag_compound.cpp` | Implements `tag_compound`'s constructor from initializer list, `at()`, `put()`, `insert()`, `erase()`, `has_key()`, `read_payload()` (reads until `tag_type::End`), and `write_payload()` (writes each entry then `tag_type::End`). |
| `src/tag_list.cpp` | Implements `tag_list`'s 12 initializer list constructors (one per tag type), the `value` initializer list constructor, `at()`, `set()`, `push_back()`, `reset()`, `read_payload()`, `write_payload()`, and `operator==`/`!=`. |
| `src/tag_string.cpp` | Implements `tag_string::read_payload()` (delegates to `reader.read_string()`) and `write_payload()` (delegates to `writer.write_string()`). |
| `src/value.cpp` | Implements `value`'s copy constructor, copy assignment, `set()`, all numeric assignment operators (using `assign_numeric_impl` helper), all numeric conversion operators (widening conversions via switch/case), string operations, and compound/list delegation methods. |
| `src/value_initializer.cpp` | Implements `value_initializer`'s constructors for each primitive type and string variants. Each constructs the appropriate tag and passes it to the `value` base. |

### I/O Sources

| File | Role |
|------|------|
| `src/endian_str.cpp` | Implements all `read_little`/`read_big`/`write_little`/`write_big` overloads. Uses byte-by-byte construction for portability (no reliance on host endianness). Float/double conversion uses `memcpy`-based type punning. Includes `static_assert` checks for `CHAR_BIT == 8`, `sizeof(float) == 4`, `sizeof(double) == 8`. |
| `src/io/stream_reader.cpp` | Implements `stream_reader::read_compound()`, `read_tag()`, `read_payload()`, `read_type()`, and `read_string()`. The `read_payload()` method tracks nesting depth with a max of `MAX_DEPTH = 1024` to prevent stack overflow from malicious input. Free functions `read_compound()` and `read_tag()` are thin wrappers. |
| `src/io/stream_writer.cpp` | Implements `stream_writer::write_tag()` (writes type + name + payload) and `write_string()` (writes 2-byte length prefix + UTF-8 data, throws `std::length_error` if string exceeds 65535 bytes). Free function `write_tag()` is a thin wrapper. |

### Compression Sources

| File | Role |
|------|------|
| `src/io/izlibstream.cpp` | Implements `inflate_streambuf`: constructor calls `inflateInit2()`, destructor calls `inflateEnd()`. The `underflow()` method reads compressed data from the wrapped stream, calls `inflate()`, and handles `Z_STREAM_END` by seeking back the wrapped stream to account for over-read data. |
| `src/io/ozlibstream.cpp` | Implements `deflate_streambuf`: constructor calls `deflateInit2()`, destructor calls `close()` then `deflateEnd()`. The `deflate_chunk()` method compresses buffered data and writes to the output stream. `close()` flushes with `Z_FINISH`. `ozlibstream::close()` handles exceptions gracefully by setting `badbit` instead of re-throwing. |

### Text Sources

| File | Role |
|------|------|
| `src/text/json_formatter.cpp` | Implements `json_formatter::print()` using a private `json_fmt_visitor` class (extends `const_nbt_visitor`). The visitor handles indentation, JSON-like output for compounds (`{}`), lists (`[]`), quoted strings, numeric suffixes (`b`, `s`, `l`, `f`, `d`), and special float values (Infinity, NaN). |

---

## Data Flow: Reading NBT

```
Input Stream
    │
    ▼
[izlibstream]  ← optional decompression
    │
    ▼
stream_reader
    ├── read_type()        → reads 1 byte, validates tag type
    ├── read_string()      → reads 2-byte length + UTF-8 name
    └── read_payload(type) → tag::create(type), then tag->read_payload(*this)
        │
        ├── tag_primitive<T>::read_payload()  → reader.read_num(value)
        ├── tag_string::read_payload()        → reader.read_string()
        ├── tag_array<T>::read_payload()      → reader.read_num(length), then read elements
        ├── tag_list::read_payload()          → read element type, length, then element payloads
        └── tag_compound::read_payload()      → loop: read_type(), read_string(), read_payload() until End
```

## Data Flow: Writing NBT

```
tag_compound (root)
    │
    ▼
stream_writer::write_tag(key, tag)
    ├── write_type(tag.get_type())     → 1 byte
    ├── write_string(key)              → 2-byte length + UTF-8
    └── write_payload(tag)             → tag.write_payload(*this)
        │
        ├── tag_primitive<T>::write_payload()  → writer.write_num(value)
        ├── tag_string::write_payload()        → writer.write_string(value)
        ├── tag_array<T>::write_payload()      → write length, then elements
        ├── tag_list::write_payload()          → write type + length + element payloads
        └── tag_compound::write_payload()      → for each entry: write_tag(key, value); write_type(End)
    │
    ▼
[ozlibstream]  ← optional compression
    │
    ▼
Output Stream
```

---

## Depth Protection

The `stream_reader` maintains a `depth` counter (private `int depth = 0`) that increments on each recursive `read_payload()` call and decrements on return. If `depth` exceeds `MAX_DEPTH` (1024), an `input_error` is thrown. This prevents stack overflow from deeply nested structures in malicious NBT files.

```cpp
std::unique_ptr<tag> stream_reader::read_payload(tag_type type)
{
    if (++depth > MAX_DEPTH)
        throw input_error("Too deeply nested");
    std::unique_ptr<tag> t = tag::create(type);
    t->read_payload(*this);
    --depth;
    return t;
}
```

---

## Export Macros

The library uses `generate_export_header()` from CMake to create `nbt_export.h` at build time. The `NBT_EXPORT` macro is applied to all public classes and functions. When building shared libraries (`NBT_BUILD_SHARED=ON`), symbols default to hidden (`CXX_VISIBILITY_PRESET hidden`) and only `NBT_EXPORT`-marked symbols are exported. For static builds, `NBT_EXPORT` expands to nothing.

---

## Numeric Widening Rules in value

The `value` class implements a strict widening hierarchy for numeric conversions:

**Assignment (write) direction** — A value can be assigned a narrower type:

```
value holding tag_short can accept int8_t (narrower OK)
value holding tag_short rejects int32_t (wider → std::bad_cast)
```

**Conversion (read) direction** — A value can be read as a wider type:

```
value holding tag_byte can be read as int8_t, int16_t, int32_t, int64_t, float, double
value holding tag_short can be read as int16_t, int32_t, int64_t, float, double
value holding tag_int can be read as int32_t, int64_t, float, double
value holding tag_long can be read as int64_t, float, double
value holding tag_float can be read as float, double
value holding tag_double can be read as double only
```

The implementation in `src/value.cpp` uses an `assign_numeric_impl` helper template with a switch-case dispatching on the existing tag type, comparing ordinal positions in the `tag_type` enum.

---

## Dependency Graph

```
nbt_tags.h ──┬── tag_primitive.h → crtp_tag.h → tag.h, nbt_visitor.h, make_unique.h
             │                   → primitive_detail.h
             │                   → io/stream_reader.h → endian_str.h, tag.h, tag_compound.h
             │                   → io/stream_writer.h → endian_str.h, tag.h
             ├── tag_string.h ──→ crtp_tag.h
             ├── tag_array.h ───→ crtp_tag.h, io/stream_reader.h, io/stream_writer.h
             ├── tag_list.h ────→ crtp_tag.h, tagfwd.h, value_initializer.h → value.h → tag.h
             └── tag_compound.h → crtp_tag.h, value_initializer.h

io/izlibstream.h → io/zlib_streambuf.h, <zlib.h>
io/ozlibstream.h → io/zlib_streambuf.h, <zlib.h>
text/json_formatter.h → tagfwd.h
```

---

## Thread Safety Implications

The architecture has no global mutable state. The `json_formatter` used by `operator<<` is a `static const` local in `tag.cpp`:

```cpp
std::ostream& operator<<(std::ostream& os, const tag& t)
{
    static const text::json_formatter formatter;
    formatter.print(os, t);
    return os;
}
```

Since `formatter` is const and `print()` is const, multiple threads can safely use `operator<<` concurrently. Tag objects themselves are not thread-safe for concurrent mutation.
