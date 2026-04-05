# Compound Tags

## Overview

`tag_compound` is the most important tag type in NBT. It represents an unordered collection of **named tags** of arbitrary types — the NBT equivalent of a JSON object or a C++ `std::map`. Every NBT file has a root compound tag, and compounds can be nested arbitrarily deep.

Defined in `include/tag_compound.h`, implemented in `src/tag_compound.cpp`.

---

## Class Definition

```cpp
class NBT_EXPORT tag_compound final : public detail::crtp_tag<tag_compound>
{
    typedef std::map<std::string, value> map_t_;

public:
    typedef map_t_::iterator iterator;
    typedef map_t_::const_iterator const_iterator;

    static constexpr tag_type type = tag_type::Compound;

    tag_compound() {}
    tag_compound(
        std::initializer_list<std::pair<std::string, value_initializer>> init);

    value& at(const std::string& key);
    const value& at(const std::string& key) const;

    value& operator[](const std::string& key);

    std::pair<iterator, bool> put(const std::string& key,
                                  value_initializer&& val);
    std::pair<iterator, bool> insert(const std::string& key,
                                     value_initializer&& val);

    template <class T, class... Args>
    std::pair<iterator, bool> emplace(const std::string& key, Args&&... args);

    bool erase(const std::string& key);
    bool has_key(const std::string& key) const;
    bool has_key(const std::string& key, tag_type type) const;

    size_t size() const;
    void clear();

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    void read_payload(io::stream_reader& reader) override;
    void write_payload(io::stream_writer& writer) const override;

    friend bool operator==(const tag_compound& lhs, const tag_compound& rhs);
    friend bool operator!=(const tag_compound& lhs, const tag_compound& rhs);

private:
    map_t_ tags;
};
```

---

## Internal Storage

`tag_compound` uses a `std::map<std::string, value>` as its internal container. This means:

- **Keys are sorted**: Iteration order is lexicographic by key name
- **Unique keys**: Each key appears at most once
- **Logarithmic access**: Lookup, insertion, and deletion are $O(\log n)$
- **Stable iterators**: Inserting/erasing does not invalidate other iterators

Each value in the map is a `value` object (wrapping `std::unique_ptr<tag>`), which owns its tag.

---

## Construction

### Default Constructor

```cpp
tag_compound comp;  // Empty compound
```

### Initializer List Constructor

The most powerful constructor takes a brace-enclosed list of key-value pairs:

```cpp
tag_compound comp{
    {"name", "Steve"},              // const char* → tag_string via value_initializer
    {"health", int16_t(20)},        // int16_t → tag_short
    {"xp", int32_t(1500)},          // int32_t → tag_int
    {"velocity", 0.0},              // double → tag_double
    {"inventory", tag_list::of<tag_compound>({
        {{"id", "minecraft:sword"}, {"count", int8_t(1)}},
        {{"id", "minecraft:shield"}, {"count", int8_t(1)}}
    })},
    {"scores", tag_int_array{100, 200, 300}},
    {"nested", tag_compound{{"inner_key", "inner_value"}}}
};
```

The initializer list type is `std::initializer_list<std::pair<std::string, value_initializer>>`. The `value_initializer` class accepts implicit conversions from all supported types (see the architecture documentation).

#### Implementation

```cpp
tag_compound::tag_compound(
    std::initializer_list<std::pair<std::string, value_initializer>> init)
{
    for (const auto& pair : init)
        tags.emplace(std::move(pair.first), std::move(pair.second));
}
```

Each pair's key and value_initializer are moved into the map. Since `value_initializer` inherits from `value`, it converts seamlessly.

---

## Element Access

### operator[] — Unchecked Access

```cpp
value& operator[](const std::string& key);
```

Returns a reference to the value associated with `key`. If the key does not exist, **a new uninitialized value is created** under that key (matching `std::map::operator[]` behavior).

```cpp
tag_compound comp{{"name", "Steve"}};

// Access existing key
value& name = comp["name"];
std::string n = static_cast<std::string>(name);  // "Steve"

// Create new entry (value is uninitialized/null)
value& newval = comp["new_key"];
// newval is a null value: (bool)newval == false
newval = int32_t(42);  // Now it holds a tag_int(42)
```

**Warning**: Since `operator[]` creates entries, do not use it to test for key existence. Use `has_key()` instead.

### at() — Bounds-Checked Access

```cpp
value& at(const std::string& key);
const value& at(const std::string& key) const;
```

Returns a reference to the value associated with `key`. Throws `std::out_of_range` if the key does not exist.

```cpp
tag_compound comp{{"health", int16_t(20)}};

value& health = comp.at("health");         // OK
const value& h = comp.at("health");        // OK (const)
comp.at("missing_key");                    // throws std::out_of_range
```

Implementation:
```cpp
value& tag_compound::at(const std::string& key)
{
    return tags.at(key);
}
```

---

## Insertion and Modification

### put() — Insert or Assign

```cpp
std::pair<iterator, bool> put(const std::string& key, value_initializer&& val);
```

If `key` already exists, **replaces** the existing value. If `key` does not exist, **inserts** a new entry. Returns a pair of (iterator to the entry, bool indicating whether the key was new).

```cpp
tag_compound comp;

auto [it1, inserted1] = comp.put("key", int32_t(42));
// inserted1 == true, comp["key"] == tag_int(42)

auto [it2, inserted2] = comp.put("key", int32_t(99));
// inserted2 == false (key existed), comp["key"] == tag_int(99)
```

Implementation:
```cpp
std::pair<tag_compound::iterator, bool>
tag_compound::put(const std::string& key, value_initializer&& val)
{
    auto it = tags.find(key);
    if (it != tags.end()) {
        it->second = std::move(val);
        return {it, false};
    } else {
        return tags.emplace(key, std::move(val));
    }
}
```

### insert() — Insert Only

```cpp
std::pair<iterator, bool> insert(const std::string& key, value_initializer&& val);
```

Inserts a new entry only if the key does **not** already exist. If the key exists, the value is not modified. Returns (iterator, bool) where bool indicates whether insertion occurred.

```cpp
auto [it, inserted] = comp.insert("key", int32_t(42));
// If "key" exists: inserted == false, value unchanged
// If "key" missing: inserted == true, "key" → tag_int(42)
```

Implementation:
```cpp
std::pair<tag_compound::iterator, bool>
tag_compound::insert(const std::string& key, value_initializer&& val)
{
    return tags.emplace(key, std::move(val));
}
```

This delegates to `std::map::emplace`, which does not overwrite existing entries.

### emplace() — Construct and Insert/Assign

```cpp
template <class T, class... Args>
std::pair<iterator, bool> emplace(const std::string& key, Args&&... args);
```

Constructs a tag of type `T` in-place and assigns or inserts it. Unlike `std::map::emplace`, this **will overwrite** existing values (it delegates to `put()`).

```cpp
// Construct a tag_int(42) in place
comp.emplace<tag_int>("key", 42);

// Construct a tag_compound with initializer
comp.emplace<tag_compound>("nested", std::initializer_list<
    std::pair<std::string, value_initializer>>{
        {"inner", int32_t(1)}
    });
```

Implementation:
```cpp
template <class T, class... Args>
std::pair<tag_compound::iterator, bool>
tag_compound::emplace(const std::string& key, Args&&... args)
{
    return put(key, value(make_unique<T>(std::forward<Args>(args)...)));
}
```

### Direct Assignment via operator[]

Since `operator[]` returns a `value&`, you can assign directly:

```cpp
comp["health"] = int16_t(20);   // Creates/updates tag_short
comp["name"] = "Steve";          // Creates/updates tag_string
comp["data"] = tag_compound{     // Assigns a whole compound
    {"nested", "value"}
};
```

Assignment behavior depends on whether the value is initialized:
- **Uninitialized value**: Creates a new tag of the appropriate type
- **Initialized value**: Updates the existing tag if types match, throws `std::bad_cast` if types differ (for numeric/string assignments on `value`)

---

## Deletion

### erase()

```cpp
bool erase(const std::string& key);
```

Removes the entry with the given key. Returns `true` if an entry was removed, `false` if the key did not exist.

```cpp
tag_compound comp{{"a", 1}, {"b", 2}, {"c", 3}};

comp.erase("b");     // returns true, comp now has "a" and "c"
comp.erase("z");     // returns false, no effect
```

Implementation:
```cpp
bool tag_compound::erase(const std::string& key)
{
    return tags.erase(key) != 0;
}
```

---

## Key Queries

### has_key() — Check Existence

```cpp
bool has_key(const std::string& key) const;
```

Returns `true` if the key exists in the compound.

```cpp
if (comp.has_key("name")) {
    // Key exists
}
```

### has_key() — Check Existence and Type

```cpp
bool has_key(const std::string& key, tag_type type) const;
```

Returns `true` if the key exists **and** the value has the specified type.

```cpp
if (comp.has_key("health", tag_type::Short)) {
    int16_t health = static_cast<int16_t>(comp.at("health"));
}
```

Implementation:
```cpp
bool tag_compound::has_key(const std::string& key, tag_type type) const
{
    auto it = tags.find(key);
    return it != tags.end() && it->second.get_type() == type;
}
```

---

## Size and Clearing

```cpp
size_t size() const { return tags.size(); }  // Number of entries
void clear() { tags.clear(); }               // Remove all entries
```

---

## Iteration

`tag_compound` provides full bidirectional iterator support over its entries. Each entry is a `std::pair<const std::string, value>`.

```cpp
iterator begin();
iterator end();
const_iterator begin() const;
const_iterator end() const;
const_iterator cbegin() const;
const_iterator cend() const;
```

### Iteration Examples

```cpp
tag_compound comp{{"a", 1}, {"b", 2}, {"c", 3}};

// Range-based for loop
for (const auto& [key, val] : comp) {
    std::cout << key << " = " << val.get() << "\n";
}
// Output (sorted by key):
// a = 1
// b = 2
// c = 3

// Iterator-based loop
for (auto it = comp.begin(); it != comp.end(); ++it) {
    std::cout << it->first << ": type=" << it->second.get_type() << "\n";
}

// Const iteration
for (auto it = comp.cbegin(); it != comp.cend(); ++it) {
    // Read-only access
}
```

**Note**: Iteration order is **lexicographic by key name** because the internal `std::map` sorts its keys. This is not necessarily the same order as the original NBT file — NBT compounds are unordered in the specification.

---

## Named Tag Insertion Patterns

### Pattern 1: Initializer List (Preferred for Construction)

```cpp
tag_compound comp{
    {"key1", int32_t(1)},
    {"key2", "hello"},
    {"key3", tag_list{1, 2, 3}}
};
```

### Pattern 2: operator[] for Dynamic Updates

```cpp
comp["new_key"] = int32_t(42);
comp["string_key"] = std::string("value");
```

### Pattern 3: put() for Insert-or-Update

```cpp
comp.put("key", int32_t(42));
comp.put("key", int32_t(99));  // Overwrites
```

### Pattern 4: insert() for Insert-if-Missing

```cpp
comp.insert("default", int32_t(42));  // Only inserts if "default" doesn't exist
```

### Pattern 5: emplace() for In-Place Construction

```cpp
comp.emplace<tag_int>("key", 42);
comp.emplace<tag_string>("name", "Steve");
```

### Pattern 6: Moving Tags In

```cpp
tag_compound inner{{"nested_key", "nested_value"}};
comp.put("section", std::move(inner));  // Moves the compound
```

---

## Binary Format

### Reading (Deserialization)

A compound tag's payload is a sequence of named tags terminated by a `tag_type::End` byte:

```
[type byte] [name length] [name bytes] [tag payload]
[type byte] [name length] [name bytes] [tag payload]
...
[0x00]  ← End tag type
```

Implementation:
```cpp
void tag_compound::read_payload(io::stream_reader& reader)
{
    clear();
    tag_type tt;
    while ((tt = reader.read_type(true)) != tag_type::End) {
        std::string key;
        try {
            key = reader.read_string();
        } catch (io::input_error& ex) {
            std::ostringstream str;
            str << "Error reading key of tag_" << tt;
            throw io::input_error(str.str());
        }
        auto tptr = reader.read_payload(tt);
        tags.emplace(std::move(key), value(std::move(tptr)));
    }
}
```

The reader loops until it encounters `tag_type::End` (0x00). For each entry:
1. Read the tag type byte
2. Read the name string (2-byte length + UTF-8)
3. Read the tag payload via `reader.read_payload()`
4. Emplace the key-value pair into the map

### Writing (Serialization)

```cpp
void tag_compound::write_payload(io::stream_writer& writer) const
{
    for (const auto& pair : tags)
        writer.write_tag(pair.first, pair.second);
    writer.write_type(tag_type::End);
}
```

The writer iterates over all entries (in map order), writing each as a named tag, then writes a single `End` byte.

---

## Equality Comparison

Two compounds are equal if and only if their internal `std::map` objects are equal:

```cpp
friend bool operator==(const tag_compound& lhs, const tag_compound& rhs)
{
    return lhs.tags == rhs.tags;
}
```

This performs a deep comparison: same keys, in the same order, with equal values (which recursively compares the owned tags).

---

## Nested Access

The `value` class delegates `operator[]` and `at()` to `tag_compound` when the held tag is a compound. This enables chained access:

```cpp
tag_compound root{
    {"player", tag_compound{
        {"name", "Steve"},
        {"stats", tag_compound{
            {"health", int16_t(20)},
            {"hunger", int16_t(18)}
        }}
    }}
};

// Chained access
std::string name = static_cast<std::string>(root["player"]["name"]);
int16_t health = static_cast<int16_t>(root["player"]["stats"]["health"]);

// Bounds-checked
root.at("player").at("stats").at("missing");  // throws std::out_of_range
```

The delegation works because `value::operator[](const std::string& key)` performs:
```cpp
value& value::operator[](const std::string& key)
{
    return dynamic_cast<tag_compound&>(*tag_)[key];
}
```

If the held tag is not a `tag_compound`, `dynamic_cast` throws `std::bad_cast`.

---

## Common Usage Patterns

### Checking and Accessing

```cpp
if (comp.has_key("version", tag_type::Int)) {
    int32_t version = static_cast<int32_t>(comp.at("version"));
}
```

### Safe Nested Access

```cpp
try {
    auto& player = comp.at("player").as<tag_compound>();
    if (player.has_key("health")) {
        int16_t health = static_cast<int16_t>(player.at("health"));
    }
} catch (const std::out_of_range& e) {
    // Key doesn't exist
} catch (const std::bad_cast& e) {
    // Type mismatch
}
```

### Building from Dynamic Data

```cpp
tag_compound comp;
for (const auto& item : items) {
    comp.put(item.name, tag_compound{
        {"id", item.id},
        {"count", int8_t(item.count)},
        {"damage", int16_t(item.damage)}
    });
}
```

### Merging Compounds

```cpp
// Copy all entries from source to dest (overwriting existing keys)
for (const auto& [key, val] : source) {
    dest.put(key, value(val));  // Explicit copy via value(const value&)
}
```
