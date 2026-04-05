# Visitor Pattern

## Overview

libnbt++ implements the classic double-dispatch visitor pattern for traversing and processing tag hierarchies without modifying the tag classes themselves. Two visitor base classes are provided: `nbt_visitor` for mutable access and `const_nbt_visitor` for read-only traversal.

Defined in `include/nbt_visitor.h`.

---

## Visitor Base Classes

### nbt_visitor — Mutable Visitor

```cpp
class nbt_visitor
{
public:
    virtual ~nbt_visitor() noexcept;

    virtual void visit(tag_byte&) {}
    virtual void visit(tag_short&) {}
    virtual void visit(tag_int&) {}
    virtual void visit(tag_long&) {}
    virtual void visit(tag_float&) {}
    virtual void visit(tag_double&) {}
    virtual void visit(tag_byte_array&) {}
    virtual void visit(tag_string&) {}
    virtual void visit(tag_list&) {}
    virtual void visit(tag_compound&) {}
    virtual void visit(tag_int_array&) {}
    virtual void visit(tag_long_array&) {}
};
```

### const_nbt_visitor — Immutable Visitor

```cpp
class const_nbt_visitor
{
public:
    virtual ~const_nbt_visitor() noexcept;

    virtual void visit(const tag_byte&) {}
    virtual void visit(const tag_short&) {}
    virtual void visit(const tag_int&) {}
    virtual void visit(const tag_long&) {}
    virtual void visit(const tag_float&) {}
    virtual void visit(const tag_double&) {}
    virtual void visit(const tag_byte_array&) {}
    virtual void visit(const tag_string&) {}
    virtual void visit(const tag_list&) {}
    virtual void visit(const tag_compound&) {}
    virtual void visit(const tag_int_array&) {}
    virtual void visit(const tag_long_array&) {}
};
```

Both provide 12 `visit()` overloads — one per concrete tag type. All default to empty (no-op), so subclasses only override the types they care about.

---

## Double Dispatch via accept()

The `tag` base class declares the `accept()` method:

```cpp
class tag
{
public:
    virtual void accept(nbt_visitor& visitor) const = 0;
    virtual void accept(const_nbt_visitor& visitor) const = 0;
};
```

The CRTP intermediate `crtp_tag<Sub>` implements both `accept()` methods:

```cpp
template <class Sub>
class crtp_tag : public tag
{
public:
    void accept(nbt_visitor& visitor) const override
    {
        visitor.visit(const_cast<Sub&>(static_cast<const Sub&>(*this)));
    }

    void accept(const_nbt_visitor& visitor) const override
    {
        visitor.visit(static_cast<const Sub&>(*this));
    }
};
```

For `nbt_visitor` (mutable), `const_cast` removes the `const` from `accept()` so the visitor receives a mutable reference.

For `const_nbt_visitor`, the const reference is passed through directly.

---

## How It Works

1. Client code creates a visitor subclass, overriding `visit()` for the types it handles
2. Client calls `tag.accept(visitor)` on any tag
3. The CRTP-generated `accept()` calls `visitor.visit(static_cast<Sub&>(*this))`
4. The correct `visit()` overload is called based on the **concrete** tag type

```
Client → tag.accept(visitor)
  → crtp_tag<tag_int>::accept()
    → visitor.visit(static_cast<tag_int&>(*this))
      → YourVisitor::visit(tag_int&)  // Your override
```

This resolves the combination of (runtime tag type) × (visitor implementation) without `dynamic_cast` or switch statements.

---

## Built-in Visitor: json_fmt_visitor

The library includes one concrete visitor in `src/text/json_formatter.cpp`:

```cpp
class json_fmt_visitor : public const_nbt_visitor
{
public:
    json_fmt_visitor(std::ostream& os, unsigned int indent);

    void visit(const tag_byte& t) override;
    void visit(const tag_short& t) override;
    void visit(const tag_int& t) override;
    void visit(const tag_long& t) override;
    void visit(const tag_float& t) override;
    void visit(const tag_double& t) override;
    void visit(const tag_byte_array& t) override;
    void visit(const tag_string& t) override;
    void visit(const tag_list& t) override;
    void visit(const tag_compound& t) override;
    void visit(const tag_int_array& t) override;
    void visit(const tag_long_array& t) override;

private:
    std::ostream& os;
    unsigned int indent;
    void write_indent();
};
```

This visitor renders any tag as a JSON-like text format. Used by `tag::operator<<` for debug output:

```cpp
std::ostream& operator<<(std::ostream& os, const tag& t)
{
    static text::json_formatter formatter;
    formatter.print(os, t);
    return os;
}
```

### Formatting Rules

| Type | Output Format | Example |
|------|--------------|---------|
| `tag_byte` | `<value>b` | `42b` |
| `tag_short` | `<value>s` | `100s` |
| `tag_int` | `<value>` | `12345` |
| `tag_long` | `<value>l` | `9876543210l` |
| `tag_float` | `<value>f` | `3.14f` |
| `tag_double` | `<value>d` | `2.718d` |
| `tag_string` | `"<value>"` | `"hello"` |
| `tag_byte_array` | `[B; ...]` | `[B; 1b, 2b, 3b]` |
| `tag_int_array` | `[I; ...]` | `[I; 1, 2, 3]` |
| `tag_long_array` | `[L; ...]` | `[L; 1l, 2l, 3l]` |
| `tag_list` | `[...]` | `[1, 2, 3]` |
| `tag_compound` | `{...}` | `{"key": 42}` |

Special float/double handling:
- `+Infinity`, `-Infinity`, `NaN` are written as-is (not JSON-compliant but accurate)
- Uses the `std::defaultfloat` format

---

## Writing Custom Visitors

### Example: Tag Counter

Count the total number of tags and tags of each type:

```cpp
class tag_counter : public const_nbt_visitor
{
public:
    int total = 0;
    std::map<tag_type, int> counts;

    void visit(const tag_byte&) override   { ++total; ++counts[tag_type::Byte]; }
    void visit(const tag_short&) override  { ++total; ++counts[tag_type::Short]; }
    void visit(const tag_int&) override    { ++total; ++counts[tag_type::Int]; }
    void visit(const tag_long&) override   { ++total; ++counts[tag_type::Long]; }
    void visit(const tag_float&) override  { ++total; ++counts[tag_type::Float]; }
    void visit(const tag_double&) override { ++total; ++counts[tag_type::Double]; }
    void visit(const tag_string&) override { ++total; ++counts[tag_type::String]; }
    void visit(const tag_byte_array&) override { ++total; ++counts[tag_type::Byte_Array]; }
    void visit(const tag_int_array&) override  { ++total; ++counts[tag_type::Int_Array]; }
    void visit(const tag_long_array&) override { ++total; ++counts[tag_type::Long_Array]; }

    void visit(const tag_list& t) override {
        ++total;
        ++counts[tag_type::List];
        for (const auto& val : t)
            val.get().accept(*this);  // Recurse into children
    }

    void visit(const tag_compound& t) override {
        ++total;
        ++counts[tag_type::Compound];
        for (const auto& [name, val] : t)
            val.get().accept(*this);  // Recurse into children
    }
};

// Usage
tag_counter counter;
root.accept(counter);
std::cout << "Total tags: " << counter.total << "\n";
```

### Example: Tag Modifier (Mutable)

Double all integer values in a tree:

```cpp
class int_doubler : public nbt_visitor
{
public:
    void visit(tag_int& t) override {
        t.set(t.get() * 2);
    }
    void visit(tag_list& t) override {
        for (auto& val : t)
            val.get().accept(*this);
    }
    void visit(tag_compound& t) override {
        for (auto& [name, val] : t)
            val.get().accept(*this);
    }
};

int_doubler doubler;
root.accept(doubler);
```

### Example: Selective Visitor

Only handle specific types — unhandled types use the default no-op:

```cpp
class string_collector : public const_nbt_visitor
{
public:
    std::vector<std::string> strings;

    void visit(const tag_string& t) override {
        strings.push_back(t.get());
    }
    void visit(const tag_list& t) override {
        for (const auto& val : t)
            val.get().accept(*this);
    }
    void visit(const tag_compound& t) override {
        for (const auto& [name, val] : t)
            val.get().accept(*this);
    }
};
```

---

## Recursive Traversal

The visitor pattern does **not** automatically recurse into compounds and lists. To walk an entire tag tree, your visitor must explicitly recurse in its `visit(tag_compound&)` and `visit(tag_list&)` overloads:

```cpp
void visit(const tag_compound& t) override {
    for (const auto& [name, val] : t)
        val.get().accept(*this);
}

void visit(const tag_list& t) override {
    for (const auto& val : t)
        val.get().accept(*this);
}
```

This is by design — it gives visitors control over traversal depth, ordering, and filtering.

---

## Visitor vs. Dynamic Cast

Two approaches to type-specific processing:

### Visitor Approach

```cpp
class my_visitor : public const_nbt_visitor {
    void visit(const tag_int& t) override { /* handle int */ }
    void visit(const tag_string& t) override { /* handle string */ }
    // ...
};
my_visitor v;
tag.accept(v);
```

### Dynamic Cast Approach

```cpp
if (auto* int_tag = dynamic_cast<const tag_int*>(&tag)) {
    // handle int
} else if (auto* str_tag = dynamic_cast<const tag_string*>(&tag)) {
    // handle string
}
```

The visitor pattern is preferable when:
- Processing many or all tag types
- Building reusable tree-walking logic
- The compiler should warn about unhandled types (though default no-ops mask this)

`dynamic_cast` / `tag::as<T>()` is simpler when:
- You know the type at the call site
- You only need to handle one or two types
- You're accessing a specific child of a compound
