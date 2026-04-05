# toml++ — Basic Usage

## Including the Library

The simplest way to start using toml++ is with the default header-only mode:

```cpp
#include <toml++/toml.hpp>
```

Or with the single-header drop-in:

```cpp
#include "toml.hpp"
```

The library places everything in the `toml` namespace. Most examples use the string literal namespace:

```cpp
using namespace std::string_view_literals;  // for "..."sv
```

---

## Parsing TOML

### Parsing a String

```cpp
#include <toml++/toml.hpp>
#include <iostream>

int main()
{
    auto tbl = toml::parse(R"(
        title = "My Config"

        [database]
        server = "192.168.1.1"
        ports = [ 8001, 8001, 8002 ]
        enabled = true
    )");

    std::cout << tbl << "\n";
    return 0;
}
```

`toml::parse()` accepts a `std::string_view` and returns a `toml::table` (when exceptions are enabled) or a `toml::parse_result` (when exceptions are disabled).

### Parsing a File

```cpp
auto tbl = toml::parse_file("config.toml");
```

`toml::parse_file()` takes a file path as `std::string_view`, opens the file, and parses its contents.

### Parsing from a Stream

```cpp
#include <fstream>

std::ifstream file("config.toml");
auto tbl = toml::parse(file, "config.toml");
```

The second argument is the source path used for error messages and `source()` metadata.

### Parsing with Source Path

You can provide a source path for diagnostic purposes:

```cpp
auto tbl = toml::parse(toml_string, "my_config.toml");
```

This path is stored in each node's `source().path` and appears in error messages.

---

## Error Handling

### With Exceptions (Default)

When `TOML_EXCEPTIONS` is enabled (the default if you don't disable them), `toml::parse()` and `toml::parse_file()` throw `toml::parse_error` on failure:

```cpp
try
{
    auto tbl = toml::parse_file("config.toml");
}
catch (const toml::parse_error& err)
{
    std::cerr << "Parse error: " << err.description() << "\n";
    std::cerr << "  at " << err.source() << "\n";
    // err.source().begin.line, err.source().begin.column
}
```

`toml::parse_error` inherits from `std::runtime_error` in this mode.

### Without Exceptions

When compiled with `TOML_EXCEPTIONS=0` (or with `-fno-exceptions`), `parse()` returns a `toml::parse_result`:

```cpp
toml::parse_result result = toml::parse_file("config.toml");

if (result)
{
    // Success — result implicitly converts to table&
    toml::table& tbl = result;
    std::cout << tbl << "\n";
}
else
{
    // Failure
    std::cerr << "Parse error: " << result.error().description() << "\n";
    std::cerr << "  at " << result.error().source() << "\n";
}
```

`parse_result` is a discriminated union that holds either a `toml::table` or a `toml::parse_error`. It converts to `bool` for success checking.

---

## Accessing Values

### Using `operator[]` — The Easy Way

`operator[]` on a `toml::table` returns a `toml::node_view`, which is a safe optional-like wrapper:

```cpp
auto tbl = toml::parse(R"(
    [server]
    host = "localhost"
    port = 8080
    debug = false
    tags = ["web", "api"]
)");

// Chained access — never throws, returns empty view if path doesn't exist
auto host_view = tbl["server"]["host"];
auto port_view = tbl["server"]["port"];

// Check if the view refers to a node
if (host_view)
    std::cout << "Host exists\n";
```

### Getting Values with `value<T>()`

```cpp
// Returns std::optional<T>
std::optional<std::string_view> host = tbl["server"]["host"].value<std::string_view>();
std::optional<int64_t> port = tbl["server"]["port"].value<int64_t>();
std::optional<bool> debug = tbl["server"]["debug"].value<bool>();

if (host)
    std::cout << "Host: " << *host << "\n";
```

`value<T>()` is permissive — it allows some type conversions (e.g., reading an integer as a double).

### Getting Values with `value_exact<T>()`

```cpp
// Strict — only returns a value if the types match exactly
std::optional<int64_t> port = tbl["server"]["port"].value_exact<int64_t>();
```

`value_exact<T>()` only succeeds if the underlying node is exactly that type. No conversions.

### Getting Values with `value_or()`

The most convenient accessor — returns the value or a default:

```cpp
std::string_view host = tbl["server"]["host"].value_or("0.0.0.0"sv);
int64_t port = tbl["server"]["port"].value_or(80);
bool debug = tbl["server"]["debug"].value_or(true);

// Safe even if the key doesn't exist:
std::string_view missing = tbl["nonexistent"]["key"].value_or("default"sv);
```

### Using `as<T>()` for Pointer Access

`as<T>()` returns a pointer to the node if it matches the type, or `nullptr`:

```cpp
if (auto* str_val = tbl["server"]["host"].as_string())
    std::cout << "Host: " << str_val->get() << "\n";

if (auto* port_val = tbl["server"]["port"].as_integer())
    std::cout << "Port: " << port_val->get() << "\n";

// Generic template version:
if (auto* arr = tbl["server"]["tags"].as<toml::array>())
    std::cout << "Tags count: " << arr->size() << "\n";
```

Specific convenience methods exist:
- `as_table()` → `toml::table*`
- `as_array()` → `toml::array*`
- `as_string()` → `toml::value<std::string>*`
- `as_integer()` → `toml::value<int64_t>*`
- `as_floating_point()` → `toml::value<double>*`
- `as_boolean()` → `toml::value<bool>*`
- `as_date()` → `toml::value<toml::date>*`
- `as_time()` → `toml::value<toml::time>*`
- `as_date_time()` → `toml::value<toml::date_time>*`

### Direct Node Access with `get()`

On `toml::table`:
```cpp
toml::node* node = tbl.get("server");
if (node && node->is_table())
{
    toml::table& server = *node->as_table();
    // ...
}
```

On `toml::array`:
```cpp
auto* arr = tbl["server"]["tags"].as_array();
if (arr && arr->size() > 0)
{
    toml::node& first = (*arr)[0];
    std::cout << first.value_or(""sv) << "\n";
}
```

### Typed get with `get_as<T>()`

```cpp
// On table — returns pointer if key exists AND matches type
if (auto* val = tbl.get_as<std::string>("title"))
    std::cout << "Title: " << val->get() << "\n";

// On array  — returns pointer if index is valid AND matches type
auto* arr = tbl["tags"].as_array();
if (arr)
{
    if (auto* s = arr->get_as<std::string>(0))
        std::cout << "First tag: " << s->get() << "\n";
}
```

---

## Iterating Tables

### Range-based For Loop

```cpp
auto tbl = toml::parse(R"(
    a = 1
    b = "hello"
    c = true
)");

for (auto&& [key, value] : tbl)
{
    std::cout << key << " = " << value << " (type: " << value.type() << ")\n";
}
```

Output:
```
a = 1 (type: integer)
b = "hello" (type: string)
c = true (type: boolean)
```

### Using `for_each()`

`for_each()` calls a visitor with each key-value pair. The value is passed as its concrete type:

```cpp
tbl.for_each([](auto& key, auto& value)
{
    std::cout << key << ": ";
    if constexpr (toml::is_string<decltype(value)>)
        std::cout << "string = " << value.get() << "\n";
    else if constexpr (toml::is_integer<decltype(value)>)
        std::cout << "integer = " << value.get() << "\n";
    else if constexpr (toml::is_boolean<decltype(value)>)
        std::cout << "boolean = " << value.get() << "\n";
    else
        std::cout << "(other)\n";
});
```

---

## Iterating Arrays

### Range-based For Loop

```cpp
auto tbl = toml::parse(R"(
    numbers = [1, 2, 3, 4, 5]
)");

auto& arr = *tbl["numbers"].as_array();
for (auto& elem : arr)
{
    std::cout << elem.value_or(0) << " ";
}
// Output: 1 2 3 4 5
```

### Index-based Access

```cpp
for (size_t i = 0; i < arr.size(); i++)
{
    std::cout << arr[i].value_or(0) << " ";
}
```

### Using `for_each()`

```cpp
arr.for_each([](size_t index, auto& elem)
{
    if constexpr (toml::is_integer<decltype(elem)>)
        std::cout << "[" << index << "] = " << elem.get() << "\n";
});
```

---

## Creating TOML Programmatically

### Constructing a Table

```cpp
auto tbl = toml::table{
    { "title", "My Application" },
    { "version", 2 },
    { "debug", false },
    { "database", toml::table{
        { "host", "localhost" },
        { "port", 5432 }
    }},
    { "tags", toml::array{ "web", "api", "rest" } }
};

std::cout << tbl << "\n";
```

Output:
```toml
title = "My Application"
version = 2
debug = false
tags = ["web", "api", "rest"]

[database]
host = "localhost"
port = 5432
```

### Inserting Values

```cpp
toml::table config;

// insert() — only inserts if key doesn't exist
config.insert("name", "MyApp");
config.insert("name", "Overwritten");  // no-op, key already exists

// insert_or_assign() — inserts or replaces
config.insert_or_assign("name", "ReplacedApp");

// emplace() — construct in place if key doesn't exist
config.emplace<std::string>("greeting", "Hello, World!");
```

### Building Arrays

```cpp
toml::array arr;
arr.push_back(1);
arr.push_back(2);
arr.push_back(3);
arr.push_back("mixed types are fine");

// Or construct directly:
auto arr2 = toml::array{ 10, 20, 30 };

// Emplace:
arr2.emplace_back<std::string>("hello");
```

### Creating Date/Time Values

```cpp
auto tbl = toml::table{
    { "birthday", toml::date{ 1990, 6, 15 } },
    { "alarm", toml::time{ 7, 30 } },
    { "event", toml::date_time{
        toml::date{ 2024, 12, 25 },
        toml::time{ 9, 0 },
        toml::time_offset{ -5, 0 }  // EST
    }}
};

std::cout << tbl << "\n";
```

Output:
```toml
birthday = 1990-06-15
alarm = 07:30:00
event = 2024-12-25T09:00:00-05:00
```

---

## Modifying Parsed Data

```cpp
auto tbl = toml::parse(R"(
    [server]
    host = "localhost"
    port = 8080
)");

// Change a value
tbl.insert_or_assign("server", toml::table{
    { "host", "0.0.0.0" },
    { "port", 443 },
    { "ssl", true }
});

// Add a new section
tbl.insert("logging", toml::table{
    { "level", "info" },
    { "file", "/var/log/app.log" }
});

// Remove a key
if (auto* server = tbl["server"].as_table())
    server->erase("ssl");

// Modify array
tbl.insert("features", toml::array{ "auth", "cache" });
if (auto* features = tbl["features"].as_array())
{
    features->push_back("logging");
    features->insert(features->begin(), "core");
}

std::cout << tbl << "\n";
```

---

## Serialization

### To TOML (Default)

Simply stream a table or use `toml_formatter`:

```cpp
// These are equivalent:
std::cout << tbl << "\n";
std::cout << toml::toml_formatter{ tbl } << "\n";
```

### To JSON

```cpp
std::cout << toml::json_formatter{ tbl } << "\n";
```

### To YAML

```cpp
std::cout << toml::yaml_formatter{ tbl } << "\n";
```

### To a String

```cpp
#include <sstream>

std::ostringstream ss;
ss << tbl;
std::string toml_string = ss.str();

// Or as JSON:
ss.str("");
ss << toml::json_formatter{ tbl };
std::string json_string = ss.str();
```

### To a File

```cpp
#include <fstream>

std::ofstream file("output.toml");
file << tbl;
```

---

## Path-Based Access

### Using `at_path()`

```cpp
auto tbl = toml::parse(R"(
    [database]
    servers = [
        { host = "alpha", port = 5432 },
        { host = "beta", port = 5433 }
    ]
)");

// Dot-separated path with array indices
auto host = toml::at_path(tbl, "database.servers[0].host");
std::cout << host.value_or("unknown"sv) << "\n";  // "alpha"

auto port = toml::at_path(tbl, "database.servers[1].port");
std::cout << port.value_or(0) << "\n";  // 5433
```

### Using `toml::path`

```cpp
toml::path p("database.servers[0].host");
auto view = tbl[p];
std::cout << view.value_or("unknown"sv) << "\n";

// Path manipulation
toml::path parent = p.parent_path();  // "database.servers[0]"
std::cout << tbl[parent] << "\n";     // { host = "alpha", port = 5432 }
```

---

## The Visitor Pattern

### Using `visit()`

```cpp
toml::node& some_node = *tbl.get("title");

some_node.visit([](auto& val)
{
    // val is the concrete type: table&, array&, or value<T>&
    using T = std::remove_cvref_t<decltype(val)>;

    if constexpr (std::is_same_v<T, toml::table>)
        std::cout << "It's a table\n";
    else if constexpr (std::is_same_v<T, toml::array>)
        std::cout << "It's an array\n";
    else
        std::cout << "It's a value: " << val.get() << "\n";
});
```

### Using `for_each()` on Tables and Arrays

`for_each()` iterates and visits each element with its concrete type:

```cpp
tbl.for_each([](const toml::key& key, auto& value)
{
    std::cout << key << " -> " << value << "\n";
});
```

---

## Source Information

Every parsed node tracks where it was defined:

```cpp
auto tbl = toml::parse_file("config.toml");

if (auto* name = tbl.get("name"))
{
    auto& src = name->source();
    std::cout << "Defined at line " << src.begin.line
              << ", column " << src.begin.column << "\n";

    if (src.path)
        std::cout << "In file: " << *src.path << "\n";
}
```

---

## Type Checking

```cpp
toml::node& node = /* some node */;

// Virtual method checks
if (node.is_string()) { /* ... */ }
if (node.is_integer()) { /* ... */ }
if (node.is_table()) { /* ... */ }

// Template check
if (node.is<double>()) { /* ... */ }
if (node.is<toml::array>()) { /* ... */ }

// Get the type enum
switch (node.type())
{
    case toml::node_type::string:         break;
    case toml::node_type::integer:        break;
    case toml::node_type::floating_point: break;
    case toml::node_type::boolean:        break;
    case toml::node_type::date:           break;
    case toml::node_type::time:           break;
    case toml::node_type::date_time:      break;
    case toml::node_type::table:          break;
    case toml::node_type::array:          break;
    default: break;
}
```

---

## Complete Example: Config File Reader

```cpp
#include <toml++/toml.hpp>
#include <iostream>
#include <string_view>

using namespace std::string_view_literals;

int main()
{
    toml::table config;
    try
    {
        config = toml::parse_file("app.toml");
    }
    catch (const toml::parse_error& err)
    {
        std::cerr << "Failed to parse config:\n" << err << "\n";
        return 1;
    }

    // Read application settings
    auto app_name = config["app"]["name"].value_or("Unknown"sv);
    auto app_version = config["app"]["version"].value_or(1);
    auto log_level = config["logging"]["level"].value_or("info"sv);
    auto log_file = config["logging"]["file"].value_or("/tmp/app.log"sv);

    std::cout << "Application: " << app_name << " v" << app_version << "\n";
    std::cout << "Log level: " << log_level << "\n";
    std::cout << "Log file: " << log_file << "\n";

    // Read database connections
    if (auto* dbs = config["databases"].as_array())
    {
        for (auto& db_node : *dbs)
        {
            if (auto* db = db_node.as_table())
            {
                auto host = (*db)["host"].value_or("localhost"sv);
                auto port = (*db)["port"].value_or(5432);
                auto name = (*db)["name"].value_or("mydb"sv);
                std::cout << "DB: " << name << " @ " << host << ":" << port << "\n";
            }
        }
    }

    // Modify and write back
    config.insert_or_assign("last_run", toml::date_time{
        toml::date{ 2024, 1, 15 },
        toml::time{ 14, 30, 0 }
    });

    std::ofstream out("app.toml");
    out << config;

    return 0;
}
```

---

## Related Documentation

- [node-system.md](node-system.md) — Deep dive into node types and value retrieval
- [tables.md](tables.md) — Table manipulation details
- [arrays.md](arrays.md) — Array manipulation details
- [parsing.md](parsing.md) — Parser internals and error handling
- [formatting.md](formatting.md) — Serialization customization
- [path-system.md](path-system.md) — Path-based navigation
