# List Tags

## Overview

`tag_list` represents an ordered collection of unnamed tags that all share the same type. It is the NBT equivalent of a typed array — all elements must be the same `tag_type`, and elements are accessed by index rather than by name.

Defined in `include/tag_list.h`, implemented in `src/tag_list.cpp`.

---

## Class Definition

```cpp
class NBT_EXPORT tag_list final : public detail::crtp_tag<tag_list>
{
public:
    typedef std::vector<value>::iterator iterator;
    typedef std::vector<value>::const_iterator const_iterator;
    static constexpr tag_type type = tag_type::List;

    template <class T> static tag_list of(std::initializer_list<T> init);

    tag_list() : tag_list(tag_type::Null) {}
    explicit tag_list(tag_type content_type) : el_type_(content_type) {}

    // Initializer list constructors for each supported type
    tag_list(std::initializer_list<int8_t> init);
    tag_list(std::initializer_list<int16_t> init);
    tag_list(std::initializer_list<int32_t> init);
    tag_list(std::initializer_list<int64_t> init);
    tag_list(std::initializer_list<float> init);
    tag_list(std::initializer_list<double> init);
    tag_list(std::initializer_list<std::string> init);
    tag_list(std::initializer_list<tag_byte_array> init);
    tag_list(std::initializer_list<tag_list> init);
    tag_list(std::initializer_list<tag_compound> init);
    tag_list(std::initializer_list<tag_int_array> init);
    tag_list(std::initializer_list<tag_long_array> init);
    tag_list(std::initializer_list<value> init);

    value& at(size_t i);
    const value& at(size_t i) const;
    value& operator[](size_t i);
    const value& operator[](size_t i) const;

    void set(size_t i, value&& val);
    void push_back(value_initializer&& val);
    template <class T, class... Args> void emplace_back(Args&&... args);
    void pop_back();

    tag_type el_type() const;
    size_t size() const;
    void clear();
    void reset(tag_type type = tag_type::Null);

    iterator begin(); iterator end();
    const_iterator begin() const; const_iterator end() const;
    const_iterator cbegin() const; const_iterator cend() const;

    void read_payload(io::stream_reader& reader) override;
    void write_payload(io::stream_writer& writer) const override;

    friend NBT_EXPORT bool operator==(const tag_list& lhs, const tag_list& rhs);
    friend NBT_EXPORT bool operator!=(const tag_list& lhs, const tag_list& rhs);

private:
    std::vector<value> tags;
    tag_type el_type_;

    template <class T, class Arg> void init(std::initializer_list<Arg> il);
};
```

---

## Internal Storage

`tag_list` stores its elements in a `std::vector<value>` and tracks the content type in `el_type_` (a `tag_type` value).

### Element Type Tracking

The `el_type_` field records what type of tags the list contains:

- **Determined**: Set to a specific `tag_type` (e.g., `tag_type::Int`) when elements are present or the type has been set explicitly
- **Undetermined**: Set to `tag_type::Null` when the list is empty and no type has been specified

The element type is automatically determined when the first element is added to an undetermined list.

---

## Construction

### Default Constructor

```cpp
tag_list()  // Empty list with undetermined type (tag_type::Null)
```

### Typed Empty Constructor

```cpp
tag_list(tag_type::Int)  // Empty list, but typed as Int
```

This is useful when you need an empty list that will later hold elements of a specific type.

### Initializer List Constructors

`tag_list` provides 12 initializer list constructors, one for each concrete element type:

```cpp
tag_list bytes{int8_t(1), int8_t(2), int8_t(3)};   // List of tag_byte
tag_list shorts{int16_t(100), int16_t(200)};         // List of tag_short
tag_list ints{1, 2, 3, 4, 5};                        // List of tag_int
tag_list longs{int64_t(1), int64_t(2)};              // List of tag_long
tag_list floats{1.0f, 2.0f, 3.0f};                   // List of tag_float
tag_list doubles{1.0, 2.0, 3.0};                     // List of tag_double
tag_list strings{"hello", "world"};                   // List of tag_string (fails: not std::string)

tag_list byte_arrays{
    tag_byte_array{1, 2, 3},
    tag_byte_array{4, 5, 6}
};  // List of tag_byte_array

tag_list nested_lists{
    tag_list{1, 2, 3},
    tag_list{4, 5, 6}
};  // List of tag_list

tag_list compounds{
    tag_compound{{"name", "a"}},
    tag_compound{{"name", "b"}}
};  // List of tag_compound
```

Each constructor delegates to the private `init<T, Arg>()` template:

```cpp
template <class T, class Arg>
void tag_list::init(std::initializer_list<Arg> init)
{
    el_type_ = T::type;
    tags.reserve(init.size());
    for (const Arg& arg : init)
        tags.emplace_back(nbt::make_unique<T>(arg));
}
```

### Value Initializer List Constructor

```cpp
tag_list(std::initializer_list<value> init);
```

Constructs a list from `value` objects. All values must be the same type, or an exception is thrown.

Implementation:
```cpp
tag_list::tag_list(std::initializer_list<value> init)
{
    if (init.size() == 0)
        el_type_ = tag_type::Null;
    else {
        el_type_ = init.begin()->get_type();
        for (const value& val : init) {
            if (!val || val.get_type() != el_type_)
                throw std::invalid_argument(
                    "The values are not all the same type");
        }
        tags.assign(init.begin(), init.end());
    }
}
```

### Static of<T>() Factory

```cpp
template <class T> static tag_list of(std::initializer_list<T> init);
```

Creates a list with elements of type `T`, where each element is constructed from the corresponding value in the initializer list. Most commonly used for creating lists of compounds:

```cpp
auto list = tag_list::of<tag_compound>({
    {{"name", "Item 1"}, {"count", int32_t(64)}},
    {{"name", "Item 2"}, {"count", int32_t(32)}}
});

auto shorts = tag_list::of<tag_short>({100, 200, 300});
auto bytes = tag_list::of<tag_byte>({1, 2, 3, 4, 5});
```

Implementation:
```cpp
template <class T> tag_list tag_list::of(std::initializer_list<T> il)
{
    tag_list result;
    result.init<T, T>(il);
    return result;
}
```

---

## Type Enforcement

`tag_list` enforces type homogeneity at runtime. Every operation that modifies the list checks that the new element matches the list's content type.

### How Type Enforcement Works

1. **Empty lists** have `el_type_ == tag_type::Null` (undetermined)
2. When the **first element** is added, `el_type_` is set to that element's type
3. Subsequent additions must have the **same type** or `std::invalid_argument` is thrown
4. `clear()` preserves the content type; `reset()` clears and optionally changes it

### Example

```cpp
tag_list list;  // el_type_ == tag_type::Null

list.push_back(int32_t(42));  // el_type_ becomes tag_type::Int
list.push_back(int32_t(99));  // OK: same type

list.push_back(int16_t(5));   // throws std::invalid_argument
// "The tag type does not match the list's content type"

list.push_back(std::string("hello"));  // throws std::invalid_argument
```

---

## Element Access

### operator[] — Unchecked Access

```cpp
value& operator[](size_t i) { return tags[i]; }
const value& operator[](size_t i) const { return tags[i]; }
```

No bounds checking. Behavior is undefined if `i >= size()`.

### at() — Bounds-Checked Access

```cpp
value& at(size_t i);
const value& at(size_t i) const;
```

Throws `std::out_of_range` if `i >= size()`.

```cpp
tag_list list{1, 2, 3};

value& first = list[0];    // tag_int(1)
value& second = list.at(1); // tag_int(2), bounds-checked

list.at(10);  // throws std::out_of_range
```

### Accessing the Contained Tag

Since each element is a `value`, you can access the underlying tag:

```cpp
tag_list list{1, 2, 3};

// Via value's conversion operators
int32_t val = static_cast<int32_t>(list[0]);

// Via as<T>()
tag_int& tag = list[0].as<tag_int>();
int32_t raw = tag.get();

// Via tag reference
const tag& t = list[0].get();
```

---

## Modification

### push_back()

```cpp
void push_back(value_initializer&& val);
```

Appends a tag to the end of the list. If the list's type is undetermined, sets it. If the type mismatches, throws `std::invalid_argument`. Null values are rejected.

```cpp
tag_list list;
list.push_back(int32_t(1));   // list is now type Int
list.push_back(int32_t(2));   // OK
list.push_back(int16_t(3));   // throws: Short != Int
```

Implementation:
```cpp
void tag_list::push_back(value_initializer&& val)
{
    if (!val)
        throw std::invalid_argument("The value must not be null");
    if (el_type_ == tag_type::Null)
        el_type_ = val.get_type();
    else if (el_type_ != val.get_type())
        throw std::invalid_argument(
            "The tag type does not match the list's content type");
    tags.push_back(std::move(val));
}
```

### emplace_back()

```cpp
template <class T, class... Args> void emplace_back(Args&&... args);
```

Constructs a tag of type `T` in-place at the end of the list. Type checking is performed against `T::type`.

```cpp
tag_list list;
list.emplace_back<tag_int>(42);
list.emplace_back<tag_int>(99);
list.emplace_back<tag_short>(5);  // throws: Short != Int
```

Implementation:
```cpp
template <class T, class... Args>
void tag_list::emplace_back(Args&&... args)
{
    if (el_type_ == tag_type::Null)
        el_type_ = T::type;
    else if (el_type_ != T::type)
        throw std::invalid_argument(
            "The tag type does not match the list's content type");
    tags.emplace_back(make_unique<T>(std::forward<Args>(args)...));
}
```

### set()

```cpp
void set(size_t i, value&& val);
```

Replaces the element at index `i`. Type checking is enforced — the new value must match `el_type_`. Throws `std::out_of_range` if the index is invalid.

```cpp
tag_list list{1, 2, 3};
list.set(1, value(tag_int(99)));  // list is now {1, 99, 3}
```

Implementation:
```cpp
void tag_list::set(size_t i, value&& val)
{
    if (val.get_type() != el_type_)
        throw std::invalid_argument(
            "The tag type does not match the list's content type");
    tags.at(i) = std::move(val);
}
```

### pop_back()

```cpp
void pop_back() { tags.pop_back(); }
```

Removes the last element. Does **not** change `el_type_`, even if the list becomes empty.

### clear()

```cpp
void clear() { tags.clear(); }
```

Removes all elements. **Preserves** the content type.

### reset()

```cpp
void reset(tag_type type = tag_type::Null);
```

Clears all elements **and** sets the content type. Defaults to `tag_type::Null` (undetermined).

```cpp
tag_list list{1, 2, 3};           // type: Int
list.reset();                      // empty, type: Null (undetermined)
list.reset(tag_type::String);      // empty, type: String
```

---

## Content Type Query

```cpp
tag_type el_type() const { return el_type_; }
```

Returns the content type of the list:
- A specific `tag_type` if determined
- `tag_type::Null` if undetermined

```cpp
tag_list list;
list.el_type();                    // tag_type::Null

tag_list ints{1, 2, 3};
ints.el_type();                    // tag_type::Int

tag_list typed(tag_type::String);
typed.el_type();                   // tag_type::String
```

---

## Iteration

`tag_list` provides full random-access iterator support over `value` elements:

```cpp
iterator begin(); iterator end();
const_iterator begin() const; const_iterator end() const;
const_iterator cbegin() const; const_iterator cend() const;
```

### Iteration Examples

```cpp
tag_list list{10, 20, 30, 40, 50};

// Range-based for
for (const auto& val : list) {
    int32_t num = static_cast<int32_t>(val);
    std::cout << num << " ";
}
// Output: 10 20 30 40 50

// Index-based
for (size_t i = 0; i < list.size(); ++i) {
    std::cout << static_cast<int32_t>(list[i]) << " ";
}

// Iterator-based
for (auto it = list.begin(); it != list.end(); ++it) {
    tag& t = it->get();
    // Process tag...
}
```

---

## Nested Access

The `value` class delegates index-based access to `tag_list` when the held tag is a list. This enables chained access from compounds:

```cpp
tag_compound root{
    {"items", tag_list::of<tag_compound>({
        {{"id", "sword"}, {"damage", int16_t(50)}},
        {{"id", "shield"}, {"damage", int16_t(100)}}
    })}
};

// Access list element from compound
value& firstItem = root["items"][0];
std::string id = static_cast<std::string>(firstItem["id"]);  // "sword"

// Bounds-checked
root["items"].at(99);  // throws std::out_of_range
```

The delegation in `value`:
```cpp
value& value::operator[](size_t i)
{
    return dynamic_cast<tag_list&>(*tag_)[i];
}

value& value::at(size_t i)
{
    return dynamic_cast<tag_list&>(*tag_).at(i);
}
```

---

## Binary Format

### Reading (Deserialization)

A list tag's payload is:

```
[element type byte] [length (4 bytes, signed)] [element payloads...]
```

Implementation:
```cpp
void tag_list::read_payload(io::stream_reader& reader)
{
    tag_type lt = reader.read_type(true);

    int32_t length;
    reader.read_num(length);
    if (length < 0)
        reader.get_istr().setstate(std::ios::failbit);
    if (!reader.get_istr())
        throw io::input_error("Error reading length of tag_list");

    if (lt != tag_type::End) {
        reset(lt);
        tags.reserve(length);
        for (int32_t i = 0; i < length; ++i)
            tags.emplace_back(reader.read_payload(lt));
    } else {
        // tag_end type: leave type undetermined
        reset(tag_type::Null);
    }
}
```

Key behaviors:
- Element type `End` (0) means an empty list with undetermined type — the length is ignored
- Negative length sets failbit and throws `io::input_error`
- Each element is read as a payload-only tag (no type byte or name)

### Writing (Serialization)

```cpp
void tag_list::write_payload(io::stream_writer& writer) const
{
    if (size() > io::stream_writer::max_array_len) {
        writer.get_ostr().setstate(std::ios::failbit);
        throw std::length_error("List is too large for NBT");
    }
    writer.write_type(el_type_ != tag_type::Null ? el_type_ : tag_type::End);
    writer.write_num(static_cast<int32_t>(size()));
    for (const auto& val : tags) {
        if (val.get_type() != el_type_) {
            writer.get_ostr().setstate(std::ios::failbit);
            throw std::logic_error(
                "The tags in the list do not all match the content type");
        }
        writer.write_payload(val);
    }
}
```

Key behaviors:
- Undetermined type (`Null`) is written as `End` (0)
- An additional consistency check verifies all elements match `el_type_` during write
- Lists exceeding `INT32_MAX` elements throw `std::length_error`

---

## Equality Comparison

Two lists are equal if they have the same element type **and** the same elements:

```cpp
bool operator==(const tag_list& lhs, const tag_list& rhs)
{
    return lhs.el_type_ == rhs.el_type_ && lhs.tags == rhs.tags;
}
```

This means:
- An empty list of `tag_type::Int` is **not** equal to an empty list of `tag_type::String`
- An empty list with undetermined type **is** equal to another undetermined empty list

---

## Common Usage Patterns

### Creating a List of Compounds (Inventory Example)

```cpp
tag_list inventory = tag_list::of<tag_compound>({
    {{"Slot", int8_t(0)}, {"id", "minecraft:diamond_sword"}, {"Count", int8_t(1)}},
    {{"Slot", int8_t(1)}, {"id", "minecraft:torch"},         {"Count", int8_t(64)}},
    {{"Slot", int8_t(2)}, {"id", "minecraft:apple"},          {"Count", int8_t(16)}}
});
```

### Building a List Dynamically

```cpp
tag_list positions;
for (const auto& pos : player_positions) {
    positions.push_back(tag_compound{
        {"x", pos.x},
        {"y", pos.y},
        {"z", pos.z}
    });
}
```

### Processing a List of Compounds

```cpp
tag_list& items = root->at("Items").as<tag_list>();
for (size_t i = 0; i < items.size(); ++i) {
    auto& item = items[i].as<tag_compound>();
    std::string id = static_cast<std::string>(item.at("id"));
    int8_t count = static_cast<int8_t>(item.at("Count"));
    std::cout << id << " x" << (int)count << "\n";
}
```

### Nested Lists

```cpp
tag_list outer = tag_list::of<tag_list>({
    tag_list{1, 2, 3},      // Inner list of Int
    tag_list{4, 5, 6}       // Inner list of Int
});

// Access: outer[0] → value wrapping tag_list{1, 2, 3}
// outer[0].as<tag_list>()[1] → tag_int(2)
```

### Converting Between List and Vector

```cpp
// List to vector
tag_list list{1, 2, 3, 4, 5};
std::vector<int32_t> vec;
for (const auto& val : list) {
    vec.push_back(static_cast<int32_t>(val));
}

// Vector to list
tag_list result;
for (int32_t v : vec) {
    result.push_back(v);
}
```

---

## Edge Cases

### Empty Lists

```cpp
tag_list empty1;                     // el_type_ == Null
tag_list empty2(tag_type::Int);      // el_type_ == Int, size == 0
tag_list empty3(tag_type::Null);     // Same as default constructor

// Read from NBT: a list with type End and length 0
// → el_type_ = Null (undetermined)
```

### Clearing vs. Resetting

```cpp
tag_list list{1, 2, 3};  // el_type_ = Int

list.clear();              // size = 0, el_type_ = Int (preserved!)
list.push_back(int32_t(4)); // OK: type still Int

list.reset();              // size = 0, el_type_ = Null
list.push_back("hello");  // OK: type becomes String
```

### Type Mismatch Prevention

```cpp
tag_list list{1, 2, 3};

// These all throw std::invalid_argument:
list.push_back(int16_t(4));               // Short != Int
list.push_back("hello");                  // String != Int
list.push_back(tag_compound{{"a", 1}});   // Compound != Int
list.set(0, value(tag_short(5)));         // Short != Int
list.emplace_back<tag_short>(5);          // Short != Int
```
