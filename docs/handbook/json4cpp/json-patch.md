# json4cpp — JSON Patch & Merge Patch

## JSON Patch (RFC 6902)

JSON Patch defines a JSON document structure for expressing a sequence of
operations to apply to a JSON document.

### `patch()`

```cpp
basic_json patch(const basic_json& json_patch) const;
```

Returns a new JSON value with the patch applied. Does not modify the
original. Throws `parse_error::104` if the patch document is malformed.

```cpp
json doc = {
    {"name", "alice"},
    {"age", 30},
    {"scores", {90, 85}}
};

json patch = json::array({
    {{"op", "replace"}, {"path", "/name"}, {"value", "bob"}},
    {{"op", "add"}, {"path", "/scores/-"}, {"value", 95}},
    {{"op", "remove"}, {"path", "/age"}}
});

json result = doc.patch(patch);
// {"name": "bob", "scores": [90, 85, 95]}
```

### `patch_inplace()`

```cpp
void patch_inplace(const basic_json& json_patch);
```

Applies the patch directly to the JSON value (modifying in place):

```cpp
json doc = {{"key", "old"}};
doc.patch_inplace(json::array({
    {{"op", "replace"}, {"path", "/key"}, {"value", "new"}}
}));
// doc is now {"key": "new"}
```

### Patch Operations

Each operation is a JSON object with an `"op"` field and operation-specific
fields:

#### `add`

Adds a value at the target location. If the target exists and is in an
object, it is replaced. If the target is in an array, the value is inserted
before the specified index.

```json
{"op": "add", "path": "/a/b", "value": 42}
```

The path's parent must exist. The `-` token appends to arrays:

```cpp
json doc = {{"arr", {1, 2}}};
json p = json::array({{{"op", "add"}, {"path", "/arr/-"}, {"value", 3}}});
doc.patch(p);  // {"arr": [1, 2, 3]}
```

#### `remove`

Removes the value at the target location:

```json
{"op": "remove", "path": "/a/b"}
```

Throws `out_of_range` if the path does not exist.

#### `replace`

Replaces the value at the target location (equivalent to `remove` + `add`):

```json
{"op": "replace", "path": "/name", "value": "bob"}
```

Throws `out_of_range` if the path does not exist.

#### `move`

Moves a value from one location to another:

```json
{"op": "move", "from": "/a/b", "path": "/c/d"}
```

Equivalent to `remove` from source + `add` to target. The `from` path
must not be a prefix of the `path`.

#### `copy`

Copies a value from one location to another:

```json
{"op": "copy", "from": "/a/b", "path": "/c/d"}
```

#### `test`

Tests that the value at the target location equals the specified value:

```json
{"op": "test", "path": "/name", "value": "alice"}
```

If the test fails, `patch()` throws `other_error::501`:

```cpp
json doc = {{"name", "alice"}};
json p = json::array({
    {{"op", "test"}, {"path", "/name"}, {"value", "bob"}}
});

try {
    doc.patch(p);
} catch (json::other_error& e) {
    // [json.exception.other_error.501] unsuccessful: ...
}
```

### Patch Validation

The `patch()` method validates each operation:
- `op` must be one of: `add`, `remove`, `replace`, `move`, `copy`, `test`
- `path` is required for all operations
- `value` is required for `add`, `replace`, `test`
- `from` is required for `move`, `copy`

Missing or invalid fields throw `parse_error::105`.

### Operation Order

Operations are applied sequentially. Each operation acts on the result of
the previous one:

```cpp
json doc = {};
json ops = json::array({
    {{"op", "add"}, {"path", "/a"}, {"value", 1}},
    {{"op", "add"}, {"path", "/b"}, {"value", 2}},
    {{"op", "replace"}, {"path", "/a"}, {"value", 10}},
    {{"op", "remove"}, {"path", "/b"}}
});

json result = doc.patch(ops);
// {"a": 10}
```

## `diff()` — Computing Patches

```cpp
static basic_json diff(const basic_json& source,
                       const basic_json& target,
                       const string_t& path = "");
```

Generates a JSON Patch that transforms `source` into `target`:

```cpp
json source = {{"name", "alice"}, {"age", 30}};
json target = {{"name", "alice"}, {"age", 31}, {"city", "wonderland"}};

json patch = json::diff(source, target);
// [
//     {"op": "replace", "path": "/age", "value": 31},
//     {"op": "add", "path": "/city", "value": "wonderland"}
// ]

// Verify roundtrip
assert(source.patch(patch) == target);
```

### Diff Algorithm

The algorithm works recursively:
1. If `source == target`, produce no operations
2. If types differ, produce a `replace` operation
3. If both are objects:
   - Keys in `source` but not `target` → `remove`
   - Keys in `target` but not `source` → `add`
   - Keys in both with different values → recurse
4. If both are arrays:
   - Compare element-by-element
   - Produce `replace` for changed elements
   - Produce `add` for extra elements in target
   - Produce `remove` for extra elements in source
5. For primitives with different values → `replace`

Note: The generated patch uses only `add`, `remove`, and `replace`
operations (not `move` or `copy`).

### Custom Base Path

The `path` parameter sets a prefix for all generated paths:

```cpp
json patch = json::diff(a, b, "/config");
// All paths will start with "/config/..."
```

## Merge Patch (RFC 7396)

Merge Patch is a simpler alternative to JSON Patch. Instead of an array of
operations, a merge patch is a JSON object that describes the desired
changes directly.

### `merge_patch()`

```cpp
void merge_patch(const basic_json& apply_patch);
```

Applies a merge patch to the JSON value in place:

```cpp
json doc = {
    {"title", "Hello"},
    {"author", {{"name", "alice"}}},
    {"tags", {"example"}}
};

json patch = {
    {"title", "Goodbye"},
    {"author", {{"name", "bob"}}},
    {"tags", nullptr}  // null means "remove"
};

doc.merge_patch(patch);
// {
//     "title": "Goodbye",
//     "author": {"name": "bob"},
// }
// "tags" was removed because the patch value was null
```

### Merge Patch Rules

The merge patch algorithm (per RFC 7396):

1. If the patch is not an object, replace the target entirely
2. If the patch is an object:
   - For each key in the patch:
     - If the value is `null`, remove the key from the target
     - Otherwise, recursively merge_patch the target's key with the value

```cpp
// Partial update — only specified fields change
json config = {{"debug", false}, {"port", 8080}, {"host", "0.0.0.0"}};

config.merge_patch({{"port", 9090}});
// {"debug": false, "port": 9090, "host": "0.0.0.0"}

config.merge_patch({{"debug", nullptr}});
// {"port": 9090, "host": "0.0.0.0"}
```

### Limitations of Merge Patch

- Cannot set a value to `null` (null means "delete")
- Cannot manipulate arrays — arrays are replaced entirely
- Cannot express "move" or "copy" semantics

```cpp
json doc = {{"items", {1, 2, 3}}};
doc.merge_patch({{"items", {4, 5}}});
// {"items": [4, 5]}  — array replaced, not merged
```

## JSON Patch vs. Merge Patch

| Feature | JSON Patch (RFC 6902) | Merge Patch (RFC 7396) |
|---|---|---|
| Format | Array of operations | JSON object |
| Operations | add, remove, replace, move, copy, test | Implicit merge |
| Array handling | Per-element operations | Replace entire array |
| Set value to null | Yes (explicit `add`/`replace`) | No (null = delete) |
| Test assertions | Yes (`test` op) | No |
| Reversibility | Can `diff()` to reverse | No |
| Complexity | More verbose | Simpler |

## Complete Example

```cpp
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

int main() {
    // Original document
    json doc = {
        {"name", "Widget"},
        {"version", "1.0"},
        {"settings", {
            {"color", "blue"},
            {"size", 10},
            {"enabled", true}
        }},
        {"tags", {"production", "stable"}}
    };

    // JSON Patch: precise operations
    json patch = json::array({
        {{"op", "replace"}, {"path", "/version"}, {"value", "2.0"}},
        {{"op", "add"}, {"path", "/settings/theme"}, {"value", "dark"}},
        {{"op", "remove"}, {"path", "/settings/size"}},
        {{"op", "add"}, {"path", "/tags/-"}, {"value", "updated"}},
        {{"op", "test"}, {"path", "/name"}, {"value", "Widget"}}
    });

    json patched = doc.patch(patch);

    // Compute diff to verify
    json computed_patch = json::diff(doc, patched);
    assert(doc.patch(computed_patch) == patched);

    // Merge Patch: simple update
    json merge = {
        {"version", "2.1"},
        {"settings", {{"color", "red"}}},
        {"tags", nullptr}  // remove tags
    };

    patched.merge_patch(merge);
    std::cout << patched.dump(2) << "\n";
}
```
