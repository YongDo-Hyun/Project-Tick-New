# toml++ — Path System

## Overview

`toml::path` provides structured navigation into a TOML document tree using dot-separated key names and array indices. Rather than chaining `operator[]` calls, a path can be constructed from a string and applied to a node tree in a single operation.

Declared in `include/toml++/impl/path.hpp` with `at_path()` free functions in `at_path.hpp`.

---

## `path_component`

Each segment of a path is a `path_component`, which is either a **key** (string) or an **array index** (integer):

```cpp
class path_component
{
  public:
    // Type query
    path_component_type type() const noexcept;
    // Returns path_component_type::key or path_component_type::array_index

    // Access (key)
    const toml::key& key() const noexcept;

    // Access (array index)
    size_t array_index() const noexcept;

    // Comparison
    friend bool operator==(const path_component&, const path_component&) noexcept;
    friend bool operator!=(const path_component&, const path_component&) noexcept;
};
```

```cpp
enum class path_component_type : uint8_t
{
    key         = 0x1,
    array_index = 0x2
};
```

### Internal Storage

`path_component` uses a union with type discrimination:

```cpp
// Simplified internal layout:
union storage_t
{
    toml::key k;        // for key components
    size_t index;       // for array_index components
};

path_component_type type_;
storage_t storage_;
```

---

## `toml::path`

A path is a sequence of `path_component` values:

```cpp
class path
{
  private:
    std::vector<path_component> components_;
};
```

### Construction

#### From String

```cpp
path(std::string_view str);
path(std::wstring_view str);  // Windows compat
```

Path string syntax:
- Dot `.` separates keys: `"server.host"` → key("server"), key("host")
- Brackets `[N]` denote array indices: `"servers[0].host"` → key("servers"), index(0), key("host")
- Quoted keys for special chars: `"a.\"dotted.key\".b"`

```cpp
toml::path p1("server.host");         // 2 components: key, key
toml::path p2("servers[0].name");     // 3 components: key, index, key
toml::path p3("[0][1]");              // 2 components: index, index
toml::path p4("database.\"dotted.key\""); // 2 components
```

#### From Components

```cpp
path();                          // empty path
path(const path& other);         // copy
path(path&& other) noexcept;     // move
```

### Size and Emptiness

```cpp
size_t size() const noexcept;     // number of components
bool empty() const noexcept;       // true if no components

explicit operator bool() const noexcept;  // true if non-empty

void clear() noexcept;            // remove all components
```

### Element Access

```cpp
path_component& operator[](size_t index) noexcept;
const path_component& operator[](size_t index) const noexcept;

// Iterator support
auto begin() noexcept;
auto end() noexcept;
auto begin() const noexcept;
auto end() const noexcept;
auto cbegin() const noexcept;
auto cend() const noexcept;
```

```cpp
toml::path p("server.ports[0]");

for (const auto& component : p)
{
    if (component.type() == toml::path_component_type::key)
        std::cout << "key: " << component.key() << "\n";
    else
        std::cout << "index: " << component.array_index() << "\n";
}
// key: server
// key: ports
// index: 0
```

---

## Path Operations

### Subpath Extraction

```cpp
path parent_path() const;        // all but last component
path leaf() const;                // last component only

path subpath(size_t start, size_t length) const;
path subpath(std::vector<path_component>::const_iterator start,
             std::vector<path_component>::const_iterator end) const;

path truncated(size_t n) const;   // first n components
```

```cpp
toml::path p("a.b.c.d");

auto parent = p.parent_path();    // "a.b.c"
auto leaf = p.leaf();             // "d"
auto sub = p.subpath(1, 2);      // "b.c"
auto trunc = p.truncated(2);     // "a.b"
```

### Concatenation

```cpp
path operator+(const path& rhs) const;
path operator+(const path_component& rhs) const;
path operator+(std::string_view rhs) const;

path& operator+=(const path& rhs);
path& operator+=(const path_component& rhs);
path& operator+=(std::string_view rhs);

// Prepend
path& prepend(const path& source);
path& prepend(path&& source);
```

```cpp
toml::path base("server");
toml::path full = base + "host";
// full == "server.host"

toml::path p("a.b");
p += "c.d";
// p == "a.b.c.d"
```

### Assignment

```cpp
path& assign(std::string_view str);
path& assign(const path& other);
path& assign(path&& other) noexcept;

path& operator=(std::string_view str);
path& operator=(const path& other);
path& operator=(path&& other) noexcept;
```

---

## Comparison

```cpp
friend bool operator==(const path& lhs, const path& rhs) noexcept;
friend bool operator!=(const path& lhs, const path& rhs) noexcept;

friend bool operator==(const path& lhs, std::string_view rhs);
friend bool operator!=(const path& lhs, std::string_view rhs);
```

```cpp
toml::path a("server.host");
toml::path b("server.host");

std::cout << (a == b) << "\n";   // true
std::cout << (a == "server.host") << "\n";  // true
```

---

## Hashing

```cpp
size_t hash() const noexcept;

// std::hash specialization
namespace std {
    template<> struct hash<toml::path> { ... };
}
```

Paths can be used as keys in `std::unordered_map` and `std::unordered_set`.

---

## String Conversion

```cpp
std::string str() const;
explicit operator std::string() const;

friend std::ostream& operator<<(std::ostream&, const path&);
```

```cpp
toml::path p("servers[0].host");
std::cout << p << "\n";           // servers[0].host
std::string s = p.str();          // "servers[0].host"
```

---

## `at_path()` — Path-Based Node Access

Declared in `include/toml++/impl/at_path.hpp`. These free functions apply a path to a node tree:

```cpp
node_view<node> at_path(node& root, const toml::path& path) noexcept;
node_view<const node> at_path(const node& root, const toml::path& path) noexcept;

node_view<node> at_path(node& root, std::string_view path) noexcept;
node_view<const node> at_path(const node& root, std::string_view path) noexcept;

// Windows compat
node_view<node> at_path(node& root, std::wstring_view path) noexcept;
```

Returns a `node_view` — null-safe wrapper that returns empty/default if the path doesn't resolve.

```cpp
auto tbl = toml::parse(R"(
    [server]
    host = "localhost"
    ports = [8080, 8081, 8082]

    [[servers]]
    name = "alpha"

    [[servers]]
    name = "beta"
)");

// Access nested value
auto host = toml::at_path(tbl, "server.host").value_or(""sv);
// "localhost"

// Access array element
auto port = toml::at_path(tbl, "server.ports[1]").value_or(int64_t{0});
// 8081

// Access array-of-tables element
auto name = toml::at_path(tbl, "servers[0].name").value_or(""sv);
// "alpha"

// Non-existent path returns empty node_view
auto missing = toml::at_path(tbl, "nonexistent.path");
std::cout << missing.value_or("default"sv) << "\n";  // "default"
```

### With `toml::path` Objects

```cpp
toml::path p("server.ports[0]");
auto port = toml::at_path(tbl, p).value_or(int64_t{0});

// Reuse path for multiple lookups
for (size_t i = 0; i < 3; i++)
{
    toml::path elem_path = toml::path("server.ports") + toml::path("[" + std::to_string(i) + "]");
    auto val = toml::at_path(tbl, elem_path).value_or(int64_t{0});
    std::cout << val << "\n";
}
```

---

## `operator[]` with Path

`table` and `node_view` also support path-like access via `operator[]`:

```cpp
auto tbl = toml::parse(R"(
    [server]
    host = "localhost"
)");

// Chained subscript (each [] does a single lookup)
auto host = tbl["server"]["host"].value_or(""sv);

// With toml::path (single lookup resolving the full path)
toml::path p("server.host");
auto host2 = toml::at_path(tbl, p).value_or(""sv);
```

Note: `operator[]` on `table` does single-key lookups only. `at_path()` resolves multi-component paths.

---

## Complete Example

```cpp
#include <toml++/toml.hpp>
#include <iostream>

int main()
{
    auto config = toml::parse(R"(
        [database]
        host = "db.example.com"
        port = 5432

        [database.pools]
        read = 10
        write = 5

        [[database.replicas]]
        host = "replica1.example.com"
        port = 5433

        [[database.replicas]]
        host = "replica2.example.com"
        port = 5434
    )");

    // Construct paths
    toml::path db_host("database.host");
    toml::path db_port("database.port");
    toml::path pool_read("database.pools.read");

    // Use at_path for access
    std::cout << "Host: " << toml::at_path(config, db_host).value_or(""sv) << "\n";
    std::cout << "Port: " << toml::at_path(config, db_port).value_or(int64_t{0}) << "\n";
    std::cout << "Read pool: " << toml::at_path(config, pool_read).value_or(int64_t{0}) << "\n";

    // Array-of-tables access
    for (size_t i = 0; i < 2; i++)
    {
        auto host_path = toml::path("database.replicas[" + std::to_string(i) + "].host");
        auto port_path = toml::path("database.replicas[" + std::to_string(i) + "].port");

        auto host = toml::at_path(config, host_path).value_or(""sv);
        auto port = toml::at_path(config, port_path).value_or(int64_t{0});

        std::cout << "Replica " << i << ": " << host << ":" << port << "\n";
    }

    // Path manipulation
    toml::path base("database");
    auto full = base + "host";
    std::cout << "Full path: " << full << "\n";       // database.host
    std::cout << "Parent: " << full.parent_path() << "\n";  // database
    std::cout << "Leaf: " << full.leaf() << "\n";       // host

    return 0;
}
```

---

## Related Documentation

- [basic-usage.md](basic-usage.md) — Simple access patterns
- [node-system.md](node-system.md) — node_view returned by at_path
- [tables.md](tables.md) — Table subscript access
