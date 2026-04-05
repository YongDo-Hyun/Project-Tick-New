# Tag System

## Overview

The tag system is the core of libnbt++. It provides a polymorphic class hierarchy where every NBT tag type maps to a concrete C++ class. All classes share the `tag` abstract base class and use the CRTP pattern via `detail::crtp_tag<Sub>` to implement common operations without repetitive boilerplate.

---

## The tag_type Enum

Defined in `include/tag.h`, `tag_type` is a strongly-typed enum representing every NBT tag type:

```cpp
enum class tag_type : int8_t {
    End        = 0,
    Byte       = 1,
    Short      = 2,
    Int        = 3,
    Long       = 4,
    Float      = 5,
    Double     = 6,
    Byte_Array = 7,
    String     = 8,
    List       = 9,
    Compound   = 10,
    Int_Array  = 11,
    Long_Array = 12,
    Null       = -1   ///< Used to denote empty value objects
};
```

### Type Validation

```cpp
bool is_valid_type(int type, bool allow_end = false);
```

Returns `true` when `type` is between 1 and 12 (inclusive), or between 0 and 12 if `allow_end` is `true`. The `End` type (0) and `Null` type (-1) are not valid for standalone tags.

### Type Output Operator

```cpp
std::ostream& operator<<(std::ostream& os, tag_type tt);
```

Outputs human-readable names: `"byte"`, `"short"`, `"int"`, `"long"`, `"float"`, `"double"`, `"byte_array"`, `"string"`, `"list"`, `"compound"`, `"int_array"`, `"long_array"`, `"end"`, `"null"`, or `"invalid"`.

---

## The tag Base Class

Defined in `include/tag.h`, `tag` is the abstract base class for all NBT tags. It declares the interface that all concrete tag classes must implement:

### Pure Virtual Methods

```cpp
virtual tag_type get_type() const noexcept = 0;          // Returns the tag type
virtual std::unique_ptr<tag> clone() const& = 0;          // Deep-copies the tag
virtual std::unique_ptr<tag> move_clone() && = 0;         // Move-constructs a copy
virtual tag& assign(tag&& rhs) = 0;                       // Move-assigns same-type tag
virtual void accept(nbt_visitor& visitor) = 0;             // Visitor pattern (mutable)
virtual void accept(const_nbt_visitor& visitor) const = 0; // Visitor pattern (const)
virtual void read_payload(io::stream_reader& reader) = 0;  // Deserialize from stream
virtual void write_payload(io::stream_writer& writer) const = 0; // Serialize to stream
```

### Non-Virtual Methods

```cpp
std::unique_ptr<tag> clone() &&;  // Rvalue overload: delegates to move_clone()
```

### Template Methods

```cpp
template <class T> T& as();
template <class T> const T& as() const;
```

Downcasts `*this` to `T&` using `dynamic_cast`. Requires `T` to be a subclass of `tag` (enforced by `static_assert`). Throws `std::bad_cast` if the tag is not of type `T`.

```cpp
// Usage:
tag& t = /* some tag */;
tag_string& s = t.as<tag_string>();  // OK if t is a tag_string
int32_t val = t.as<tag_int>().get(); // OK if t is a tag_int
```

### Factory Methods

These static methods construct tags at runtime by `tag_type`:

```cpp
static std::unique_ptr<tag> create(tag_type type);  // Default-construct
```

Creates a new tag with default values:
- Numeric types: value = 0
- String: empty string
- Arrays: empty vector
- List: empty list (undetermined type)
- Compound: empty compound

Throws `std::invalid_argument` for `tag_type::End`, `tag_type::Null`, or invalid values.

```cpp
static std::unique_ptr<tag> create(tag_type type, int8_t val);
static std::unique_ptr<tag> create(tag_type type, int16_t val);
static std::unique_ptr<tag> create(tag_type type, int32_t val);
static std::unique_ptr<tag> create(tag_type type, int64_t val);
static std::unique_ptr<tag> create(tag_type type, float val);
static std::unique_ptr<tag> create(tag_type type, double val);
```

Creates a numeric tag with the specified value. The value is cast to the appropriate type. Throws `std::invalid_argument` if `type` is not a numeric type (Byte through Double).

```cpp
auto t = tag::create(tag_type::Int);           // tag_int(0)
auto t = tag::create(tag_type::Float, 3.14f);  // tag_float(3.14)
auto t = tag::create(tag_type::Byte, 42);      // tag_byte(42), value cast to int8_t
```

### Equality Operators

```cpp
friend bool operator==(const tag& lhs, const tag& rhs);
friend bool operator!=(const tag& lhs, const tag& rhs);
```

The `operator==` implementation first checks `typeid(lhs) != typeid(rhs)` — if the RTTI types differ, tags are unequal. If types match, it delegates to the private virtual `equals()` method, which each concrete class implements via the CRTP.

### Output Operator

```cpp
std::ostream& operator<<(std::ostream& os, const tag& t);
```

Uses `text::json_formatter` to produce JSON-like output. Created as a `static const` in `src/tag.cpp`.

---

## Concrete Tag Classes

### tag_byte / tag_short / tag_int / tag_long / tag_float / tag_double

These are all instantiations of `tag_primitive<T>`, defined in `include/tag_primitive.h`:

```cpp
template <class T>
class tag_primitive final : public detail::crtp_tag<tag_primitive<T>>
{
public:
    typedef T value_type;
    static constexpr tag_type type = detail::get_primitive_type<T>::value;

    constexpr tag_primitive(T val = 0) noexcept : value(val) {}

    // Implicit conversion to/from T
    operator T&();
    constexpr operator T() const;
    constexpr T get() const { return value; }

    tag_primitive& operator=(T val) { value = val; return *this; }
    void set(T val) { value = val; }

    void read_payload(io::stream_reader& reader) override;
    void write_payload(io::stream_writer& writer) const override;

private:
    T value;
};
```

#### Type Mapping

| Typedef       | Template                 | `type` constant      | C++ Type   | NBT Size |
|--------------|--------------------------|----------------------|------------|----------|
| `tag_byte`   | `tag_primitive<int8_t>`  | `tag_type::Byte`     | `int8_t`   | 1 byte   |
| `tag_short`  | `tag_primitive<int16_t>` | `tag_type::Short`    | `int16_t`  | 2 bytes  |
| `tag_int`    | `tag_primitive<int32_t>` | `tag_type::Int`      | `int32_t`  | 4 bytes  |
| `tag_long`   | `tag_primitive<int64_t>` | `tag_type::Long`     | `int64_t`  | 8 bytes  |
| `tag_float`  | `tag_primitive<float>`   | `tag_type::Float`    | `float`    | 4 bytes  |
| `tag_double` | `tag_primitive<double>`  | `tag_type::Double`   | `double`   | 8 bytes  |

#### Implicit Conversions

`tag_primitive<T>` provides implicit conversion operators. This allows natural C++ usage:

```cpp
tag_int myInt(42);
int val = myInt;        // Implicit conversion: constexpr operator T() const
int& ref = myInt;       // Mutable reference: operator T&()
ref = 100;              // Modifies the tag's value
myInt = 200;            // Uses operator=(T val)
```

#### Binary I/O

Reading and writing are implemented inline in the header:

```cpp
template <class T>
void tag_primitive<T>::read_payload(io::stream_reader& reader)
{
    reader.read_num(value);
    if (!reader.get_istr()) {
        std::ostringstream str;
        str << "Error reading tag_" << type;
        throw io::input_error(str.str());
    }
}

template <class T>
void tag_primitive<T>::write_payload(io::stream_writer& writer) const
{
    writer.write_num(value);
}
```

#### Equality

```cpp
template <class T>
bool operator==(const tag_primitive<T>& lhs, const tag_primitive<T>& rhs)
{
    return lhs.get() == rhs.get();
}
```

Note: `tag_float(2.5)` and `tag_double(2.5)` are **not** equal — they are different types.

#### Explicit Instantiation

In `include/tag_primitive.h`:
```cpp
extern template class NBT_EXPORT tag_primitive<int8_t>;
extern template class NBT_EXPORT tag_primitive<int16_t>;
extern template class NBT_EXPORT tag_primitive<int32_t>;
extern template class NBT_EXPORT tag_primitive<int64_t>;
extern template class NBT_EXPORT tag_primitive<float>;
extern template class NBT_EXPORT tag_primitive<double>;
```

In `src/tag.cpp`:
```cpp
template class tag_primitive<int8_t>;
template class tag_primitive<int16_t>;
template class tag_primitive<int32_t>;
template class tag_primitive<int64_t>;
template class tag_primitive<float>;
template class tag_primitive<double>;
```

This ensures template code is compiled once in `tag.cpp` rather than in every translation unit.

---

### tag_string

Defined in `include/tag_string.h`:

```cpp
class NBT_EXPORT tag_string final : public detail::crtp_tag<tag_string>
{
public:
    static constexpr tag_type type = tag_type::String;

    tag_string() {}
    tag_string(const std::string& str) : value(str) {}
    tag_string(std::string&& str) noexcept : value(std::move(str)) {}
    tag_string(const char* str) : value(str) {}

    // Implicit conversion to/from std::string
    operator std::string&();
    operator const std::string&() const;
    const std::string& get() const { return value; }

    tag_string& operator=(const std::string& str);
    tag_string& operator=(std::string&& str);
    tag_string& operator=(const char* str);
    void set(const std::string& str);
    void set(std::string&& str);

    void read_payload(io::stream_reader& reader) override;
    void write_payload(io::stream_writer& writer) const override;

private:
    std::string value;
};
```

#### NBT String Format

NBT strings are encoded as:
1. A 2-byte unsigned big-endian length prefix (max 65535)
2. UTF-8 encoded characters

The `write_payload` method throws `std::length_error` if the string exceeds 65535 bytes.

#### Usage

```cpp
tag_string name("Steve");
std::string& ref = name;     // Implicit conversion
ref = "Alex";                 // Modifies in place
name = "Notch";               // operator=(const char*)
name.set("jeb_");             // Explicit setter
```

---

### tag_byte_array / tag_int_array / tag_long_array

These are all instantiations of `tag_array<T>`, defined in `include/tag_array.h`:

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
    tag_array(std::initializer_list<T> init) : data(init) {}
    tag_array(std::vector<T>&& vec) noexcept : data(std::move(vec)) {}

    std::vector<T>& get();
    const std::vector<T>& get() const;

    T& at(size_t i);       // Bounds-checked
    T& operator[](size_t i); // Unchecked

    void push_back(T val);
    void pop_back();
    size_t size() const;
    void clear();

    iterator begin(); iterator end();
    const_iterator begin() const; const_iterator end() const;
    const_iterator cbegin() const; const_iterator cend() const;

    void read_payload(io::stream_reader& reader) override;
    void write_payload(io::stream_writer& writer) const override;

private:
    std::vector<T> data;
};
```

#### Type Mapping

| Typedef            | Template              | `type` constant         |
|-------------------|-----------------------|------------------------|
| `tag_byte_array`  | `tag_array<int8_t>`   | `tag_type::Byte_Array` |
| `tag_int_array`   | `tag_array<int32_t>`  | `tag_type::Int_Array`  |
| `tag_long_array`  | `tag_array<int64_t>`  | `tag_type::Long_Array` |

#### Specialized Binary I/O

The `read_payload` and `write_payload` methods have three implementations:

**Byte arrays** (`tag_array<int8_t>`): Read/written as raw byte blocks:
```cpp
template <>
void tag_array<int8_t>::read_payload(io::stream_reader& reader)
{
    int32_t length;
    reader.read_num(length);
    if (length < 0)
        reader.get_istr().setstate(std::ios::failbit);
    if (!reader.get_istr())
        throw io::input_error("Error reading length of tag_byte_array");

    data.resize(length);
    reader.get_istr().read(reinterpret_cast<char*>(data.data()), length);
    if (!reader.get_istr())
        throw io::input_error("Error reading contents of tag_byte_array");
}
```

**Long arrays** (`tag_array<int64_t>`): Read element-by-element with `read_num`:
```cpp
template <>
void tag_array<int64_t>::read_payload(io::stream_reader& reader)
{
    int32_t length;
    reader.read_num(length);
    if (length < 0) reader.get_istr().setstate(std::ios::failbit);
    if (!reader.get_istr())
        throw io::input_error("Error reading length of tag_long_array");

    data.clear();
    data.reserve(length);
    for (int32_t i = 0; i < length; ++i) {
        int64_t val;
        reader.read_num(val);
        data.push_back(val);
    }
    if (!reader.get_istr())
        throw io::input_error("Error reading contents of tag_long_array");
}
```

**Int arrays and generic** (`tag_array<T>`): Uses the generic template with `read_num`:
```cpp
template <typename T>
void tag_array<T>::read_payload(io::stream_reader& reader)
{
    int32_t length;
    reader.read_num(length);
    // ... similar element-by-element reading ...
}
```

#### NBT Array Format

Arrays are encoded as:
1. A 4-byte signed big-endian length (number of elements)
2. The elements in sequence

Negative lengths set the failbit and throw `io::input_error`. Arrays exceeding `INT32_MAX` elements throw `std::length_error` on write.

#### Usage

```cpp
// Initialize with values
tag_byte_array ba{0, 1, 2, 3, 4};
tag_int_array ia{100, 200, 300};
tag_long_array la{1000000000LL, 2000000000LL};

// Access elements
int8_t first = ba[0];
int32_t safe = ia.at(1);  // Bounds-checked

// Modify
ba.push_back(5);
ia.pop_back();
la.clear();

// Iterate
for (int32_t val : ia) {
    std::cout << val << " ";
}

// Access underlying vector
std::vector<int8_t>& raw = ba.get();
raw.insert(raw.begin(), -1);
```

---

### tag_list

See [list-tags.md](list-tags.md) for full details. Briefly: `tag_list` stores a `std::vector<value>` with a tracked element type (`el_type_`). All elements must have the same type.

### tag_compound

See [compound-tags.md](compound-tags.md) for full details. Briefly: `tag_compound` stores a `std::map<std::string, value>` providing named tag access with ordered iteration.

---

## Clone, Equals, and Assign

These operations are all provided by the CRTP layer (`detail::crtp_tag<Sub>`) and work uniformly across all tag types:

### clone()

```cpp
std::unique_ptr<tag> clone() const&;   // Copy-clones
std::unique_ptr<tag> move_clone() &&;  // Move-clones
std::unique_ptr<tag> clone() &&;       // Delegates to move_clone()
```

The CRTP implementation:
```cpp
std::unique_ptr<tag> clone() const& override final {
    return make_unique<Sub>(sub_this());  // Copy constructor of Sub
}
std::unique_ptr<tag> move_clone() && override final {
    return make_unique<Sub>(std::move(sub_this()));  // Move constructor of Sub
}
```

**Example:**
```cpp
tag_compound comp{{"key", 42}};

// Deep copy
auto copy = comp.clone();
// copy is a tag_compound with {"key": tag_int(42)}

// Move (original is in moved-from state)
auto moved = std::move(comp).clone();
```

### equals()

The private virtual `equals()` method (implemented by crtp_tag) delegates to the concrete class's `operator==`:

```cpp
bool equals(const tag& rhs) const override final {
    return sub_this() == static_cast<const Sub&>(rhs);
}
```

The public `operator==` first checks RTTI types:
```cpp
bool operator==(const tag& lhs, const tag& rhs)
{
    if (typeid(lhs) != typeid(rhs))
        return false;
    return lhs.equals(rhs);
}
```

**Examples:**
```cpp
tag_int a(42), b(42), c(99);
a == b;  // true (same type, same value)
a == c;  // false (same type, different value)

tag_float f(42.0f);
a == f;  // false (different types, even though numeric value matches)
```

### assign()

The CRTP implementation:
```cpp
tag& assign(tag&& rhs) override final {
    return sub_this() = dynamic_cast<Sub&&>(rhs);
}
```

This move-assigns the content of `rhs` into `*this`. Throws `std::bad_cast` if `rhs` is not the same concrete type as `*this`.

**Example:**
```cpp
tag_string s("hello");
s.assign(tag_string("world"));  // OK: s now contains "world"

tag_int i(42);
s.assign(std::move(i));         // throws std::bad_cast (int != string)
```

---

## Primitive Type Traits

The `detail::get_primitive_type<T>` meta-struct (`include/primitive_detail.h`) uses template specialization to map C++ types to `tag_type` values at compile time:

```cpp
namespace detail {
    template <class T> struct get_primitive_type {
        static_assert(sizeof(T) != sizeof(T),
                      "Invalid type paramter for tag_primitive");
    };

    template <> struct get_primitive_type<int8_t>
        : public std::integral_constant<tag_type, tag_type::Byte>    {};
    template <> struct get_primitive_type<int16_t>
        : public std::integral_constant<tag_type, tag_type::Short>   {};
    template <> struct get_primitive_type<int32_t>
        : public std::integral_constant<tag_type, tag_type::Int>     {};
    template <> struct get_primitive_type<int64_t>
        : public std::integral_constant<tag_type, tag_type::Long>    {};
    template <> struct get_primitive_type<float>
        : public std::integral_constant<tag_type, tag_type::Float>   {};
    template <> struct get_primitive_type<double>
        : public std::integral_constant<tag_type, tag_type::Double>  {};
}
```

The unspecialized template uses a `static_assert` that always fails (via `sizeof(T) != sizeof(T)`, which is always `false`). This ensures that attempting to create a `tag_primitive<SomeOtherType>` produces a clear compile error.

Similarly, `detail::get_array_type<T>` maps array element types:

```cpp
template <> struct get_array_type<int8_t>
    : public std::integral_constant<tag_type, tag_type::Byte_Array> {};
template <> struct get_array_type<int32_t>
    : public std::integral_constant<tag_type, tag_type::Int_Array>  {};
template <> struct get_array_type<int64_t>
    : public std::integral_constant<tag_type, tag_type::Long_Array> {};
```

---

## make_unique Helper

Defined in `include/make_unique.h`, this provides a C++11 polyfill for `std::make_unique` (which was introduced in C++14):

```cpp
namespace nbt {
    template <class T, class... Args>
    std::unique_ptr<T> make_unique(Args&&... args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}
```

It is used throughout the library for creating tag instances:
```cpp
auto t = nbt::make_unique<tag_int>(42);
auto s = nbt::make_unique<tag_string>("hello");
```

---

## Tag Construction Summary

| Tag Type        | Default Constructor | Value Constructor                                | Initializer List              |
|----------------|---------------------|--------------------------------------------------|-------------------------------|
| `tag_byte`     | `tag_byte()`→0     | `tag_byte(int8_t(42))`                           | N/A                           |
| `tag_short`    | `tag_short()`→0    | `tag_short(int16_t(1000))`                       | N/A                           |
| `tag_int`      | `tag_int()`→0      | `tag_int(42)`                                     | N/A                           |
| `tag_long`     | `tag_long()`→0     | `tag_long(int64_t(123456789))`                   | N/A                           |
| `tag_float`    | `tag_float()`→0    | `tag_float(3.14f)`                               | N/A                           |
| `tag_double`   | `tag_double()`→0   | `tag_double(2.71828)`                            | N/A                           |
| `tag_string`   | `tag_string()`→""  | `tag_string("text")`, `tag_string(std::string)`  | N/A                           |
| `tag_byte_array`| `tag_byte_array()` | `tag_byte_array(std::vector<int8_t>&&)`          | `tag_byte_array{1,2,3}`      |
| `tag_int_array` | `tag_int_array()`  | `tag_int_array(std::vector<int32_t>&&)`           | `tag_int_array{1,2,3}`       |
| `tag_long_array`| `tag_long_array()` | `tag_long_array(std::vector<int64_t>&&)`          | `tag_long_array{1,2,3}`      |
| `tag_list`     | `tag_list()`→Null  | `tag_list(tag_type)` (empty with type)            | `tag_list{1,2,3}` (various)  |
| `tag_compound` | `tag_compound()`   | N/A                                               | `tag_compound{{"k",v},...}`   |

---

## Error Handling in Tags

| Operation          | Exception                 | Condition                           |
|-------------------|---------------------------|-------------------------------------|
| `tag::create()`   | `std::invalid_argument`   | Invalid type (End, Null, or >12)    |
| `tag::as<T>()`    | `std::bad_cast`           | Tag is not of type T                |
| `tag::assign()`   | `std::bad_cast`           | Source tag has different type        |
| Primitive I/O     | `io::input_error`         | Stream read failure                 |
| String write      | `std::length_error`       | String exceeds 65535 bytes          |
| Array read        | `io::input_error`         | Negative length or read failure     |
| Array write       | `std::length_error`       | Array exceeds INT32_MAX elements    |
