# json4cpp — Custom Type Serialization

## ADL-Based Serialization

The library uses **Argument-Dependent Lookup** (ADL) to find `to_json()`
and `from_json()` free functions for user-defined types. This allows
seamless conversion without modifying the library.

### Basic Pattern

Define `to_json()` and `from_json()` as free functions in the **same
namespace** as your type:

```cpp
namespace myapp {

struct Person {
    std::string name;
    int age;
};

void to_json(nlohmann::json& j, const Person& p) {
    j = nlohmann::json{{"name", p.name}, {"age", p.age}};
}

void from_json(const nlohmann::json& j, Person& p) {
    j.at("name").get_to(p.name);
    j.at("age").get_to(p.age);
}

}  // namespace myapp
```

Usage:

```cpp
myapp::Person alice{"alice", 30};

// Serialization
json j = alice;          // calls myapp::to_json via ADL
// or
json j2;
j2 = alice;

// Deserialization
auto bob = j.get<myapp::Person>();  // calls myapp::from_json via ADL
// or
myapp::Person carol;
j.get_to(carol);
```

### How ADL Resolution Works

When you write `json j = my_obj;`, the library calls:

```cpp
nlohmann::adl_serializer<MyType>::to_json(j, my_obj);
```

The default `adl_serializer` implementation delegates via an unqualified
call:

```cpp
template<typename BasicJsonType, typename TargetType>
static auto to_json(BasicJsonType& j, TargetType&& val)
    -> decltype(::nlohmann::to_json(j, std::forward<TargetType>(val)), void())
{
    ::nlohmann::to_json(j, std::forward<TargetType>(val));
}
```

The unqualified call to `::nlohmann::to_json(j, val)` finds:
1. Built-in overloads in `namespace nlohmann` (via `using` declarations)
2. User-provided overloads in the type's namespace (via ADL)

## `get_to()` Helper

```cpp
template<typename ValueType>
ValueType& get_to(ValueType& v) const;
```

Converts and writes into an existing variable:

```cpp
json j = {{"x", 1}, {"y", 2}};
int x, y;
j.at("x").get_to(x);
j.at("y").get_to(y);
```

## Automatic Macros

The library provides macros to auto-generate `to_json()` and `from_json()`
without writing them manually. All macros are defined in
`include/nlohmann/detail/macro_scope.hpp`.

### `NLOHMANN_DEFINE_TYPE_INTRUSIVE`

Defines `to_json()` and `from_json()` as **friend functions** inside the
class body. Requires all fields to be present during deserialization:

```cpp
struct Point {
    double x;
    double y;
    double z;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Point, x, y, z)
};
```

This expands to:

```cpp
friend void to_json(nlohmann::json& nlohmann_json_j, const Point& nlohmann_json_t) {
    nlohmann_json_j["x"] = nlohmann_json_t.x;
    nlohmann_json_j["y"] = nlohmann_json_t.y;
    nlohmann_json_j["z"] = nlohmann_json_t.z;
}

friend void from_json(const nlohmann::json& nlohmann_json_j, Point& nlohmann_json_t) {
    nlohmann_json_j.at("x").get_to(nlohmann_json_t.x);
    nlohmann_json_j.at("y").get_to(nlohmann_json_t.y);
    nlohmann_json_j.at("z").get_to(nlohmann_json_t.z);
}
```

### `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`

Same as above, but uses `value()` instead of `at()` during deserialization.
Missing keys get the default-constructed or current value instead of
throwing:

```cpp
struct Config {
    std::string host = "localhost";
    int port = 8080;
    bool debug = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, host, port, debug)
};
```

Now parsing `{}` produces a Config with all default values instead of
throwing.

### `NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE`

Generates only the `to_json()` function (no `from_json()`). Useful for
types that should be serializable but not deserializable:

```cpp
struct LogEntry {
    std::string timestamp;
    std::string message;
    int level;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(LogEntry, timestamp, message, level)
};
```

### `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`

Defines `to_json()` and `from_json()` as **free functions** outside the
class. Requires all members to be public:

```cpp
struct Color {
    int r, g, b;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, r, g, b)
```

### `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT`

Non-intrusive version with default values for missing keys:

```cpp
struct Margin {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Margin, top, right, bottom, left)
```

### `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE`

Non-intrusive, serialize-only:

```cpp
struct Metric {
    std::string name;
    double value;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(Metric, name, value)
```

## Derived Type Macros

For inheritance hierarchies, use the `DERIVED_TYPE` variants. These include
the base class fields:

### `NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE`

```cpp
struct Base {
    std::string id;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Base, id)
};

struct Derived : Base {
    int value;
    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(Derived, Base, value)
};
```

This generates serialization that includes both `id` (from Base) and
`value` (from Derived).

### All Derived Variants

| Macro | Intrusive | Default | Serialize-Only |
|---|---|---|---|
| `NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE` | Yes | No | No |
| `NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE_WITH_DEFAULT` | Yes | Yes | No |
| `NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE_ONLY_SERIALIZE` | Yes | — | Yes |
| `NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE` | No | No | No |
| `NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE_WITH_DEFAULT` | No | Yes | No |
| `NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE` | No | — | Yes |

## Low-Level Macros

### `NLOHMANN_JSON_TO` / `NLOHMANN_JSON_FROM`

Building-block macros for custom serialization:

```cpp
#define NLOHMANN_JSON_TO(v1)   nlohmann_json_j[#v1] = nlohmann_json_t.v1;
#define NLOHMANN_JSON_FROM(v1) nlohmann_json_j.at(#v1).get_to(nlohmann_json_t.v1);
#define NLOHMANN_JSON_FROM_WITH_DEFAULT(v1) \
    nlohmann_json_t.v1 = nlohmann_json_j.value(#v1, nlohmann_json_default_obj.v1);
```

These are used internally by the `NLOHMANN_DEFINE_TYPE_*` macros and can
be used directly for custom patterns.

## Custom `adl_serializer` Specialization

For types where you can't add free functions (e.g., third-party types),
specialize `adl_serializer`:

```cpp
namespace nlohmann {

template<>
struct adl_serializer<third_party::Point3D> {
    static void to_json(json& j, const third_party::Point3D& p) {
        j = json{{"x", p.x()}, {"y", p.y()}, {"z", p.z()}};
    }

    static void from_json(const json& j, third_party::Point3D& p) {
        p = third_party::Point3D(
            j.at("x").get<double>(),
            j.at("y").get<double>(),
            j.at("z").get<double>()
        );
    }
};

}  // namespace nlohmann
```

### Non-Default-Constructible Types

For types without a default constructor, implement `from_json()` as a
static method returning the constructed value:

```cpp
namespace nlohmann {

template<>
struct adl_serializer<Immutable> {
    static Immutable from_json(const json& j) {
        return Immutable(j.at("x").get<int>(), j.at("y").get<int>());
    }

    static void to_json(json& j, const Immutable& val) {
        j = json{{"x", val.x()}, {"y", val.y()}};
    }
};

}  // namespace nlohmann
```

Usage:

```cpp
json j = {{"x", 1}, {"y", 2}};
auto val = j.get<Immutable>();  // calls adl_serializer<Immutable>::from_json(j)
```

## Enum Serialization

### Default: Integer Mapping

By default, enums are serialized as their underlying integer value:

```cpp
enum class Status { active, inactive, pending };

json j = Status::active;  // 0
auto s = j.get<Status>();  // Status::active
```

### Disabling Enum Serialization

```cpp
#define JSON_DISABLE_ENUM_SERIALIZATION 1
```

Or via CMake:

```cmake
set(JSON_DisableEnumSerialization ON)
```

### Custom Enum Mapping with `NLOHMANN_JSON_SERIALIZE_ENUM`

```cpp
enum class Color { red, green, blue };

NLOHMANN_JSON_SERIALIZE_ENUM(Color, {
    {Color::red,   "red"},
    {Color::green, "green"},
    {Color::blue,  "blue"},
})
```

This generates both `to_json()` and `from_json()` that map between enum
values and strings:

```cpp
json j = Color::red;   // "red"
auto c = j.get<Color>(); // Color::red

json j2 = "unknown";
auto c2 = j2.get<Color>();  // Color::red (first entry is the default)
```

The first entry in the mapping serves as the default for unrecognized
values during deserialization.

## Nested Types

```cpp
struct Address {
    std::string city;
    std::string zip;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Address, city, zip)
};

struct Employee {
    std::string name;
    Address address;
    std::vector<std::string> skills;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Employee, name, address, skills)
};
```

The library handles nesting automatically — `Address` has its own
serialization, and `Employee` uses it implicitly:

```cpp
Employee emp{"alice", {"wonderland", "12345"}, {"c++", "python"}};
json j = emp;
// {
//     "name": "alice",
//     "address": {"city": "wonderland", "zip": "12345"},
//     "skills": ["c++", "python"]
// }

auto emp2 = j.get<Employee>();
```

## Optional Fields

Use `std::optional` (C++17) for truly optional fields:

```cpp
struct UserProfile {
    std::string username;
    std::optional<std::string> bio;
    std::optional<int> age;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UserProfile, username, bio, age)
};
```

`std::optional<T>` serializes as:
- The value `T` if it has a value
- `null` if it's `std::nullopt`

With `_WITH_DEFAULT`, missing keys leave the optional as `std::nullopt`.

## Collection Types

Standard containers are automatically handled:

```cpp
struct Team {
    std::string name;
    std::vector<Person> members;
    std::map<std::string, int> scores;
    std::set<std::string> tags;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Team, name, members, scores, tags)
};
```

Any type that satisfies the required type traits is automatically
serializable:
- Sequence containers (`std::vector`, `std::list`, `std::deque`, etc.)
- Associative containers (`std::map`, `std::set`, `std::unordered_map`, etc.)
- `std::pair`, `std::tuple`
- `std::array`

## Smart Pointers

`std::unique_ptr<T>` and `std::shared_ptr<T>` are supported if `T` is
serializable:

```cpp
struct Node {
    int value;
    std::shared_ptr<Node> next;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Node, value, next)
};
```

- Non-null pointer → serializes the pointed-to value
- Null pointer → serializes as JSON `null`
- JSON `null` → deserializes as null pointer

## Type Traits

The library uses SFINAE-based type traits to detect capabilities:

| Trait | Purpose |
|---|---|
| `is_compatible_type` | Can be converted to/from JSON |
| `has_to_json` | Has a `to_json()` function |
| `has_from_json` | Has a `from_json()` function |
| `is_compatible_object_type` | Looks like a JSON object |
| `is_compatible_array_type` | Looks like a JSON array |
| `is_compatible_string_type` | Looks like a JSON string |
| `is_compatible_integer_type` | Looks like a JSON integer |

These traits live in `include/nlohmann/detail/meta/type_traits.hpp`.
