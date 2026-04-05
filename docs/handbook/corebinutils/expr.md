# expr — Evaluate Expressions

## Overview

`expr` evaluates arithmetic, string, and logical expressions from the command
line and writes the result to standard output. It implements a recursive
descent parser with automatic type coercion between strings, numeric strings,
and integers.

**Source**: `expr/expr.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
expr expression
```

## Source Analysis

### Value Types

```c
enum value_type {
    INTEGER,         /* Pure integer (from arithmetic) */
    NUMERIC_STRING,  /* String that looks like a number */
    STRING,          /* General string */
};

struct value {
    enum value_type type;
    union {
        intmax_t ival;
        char *sval;
    };
};
```

`expr` automatically coerces between types during operations. A value
like `"42"` starts as `NUMERIC_STRING` and is promoted to `INTEGER` for
arithmetic.

### Parser Architecture

`expr` uses a recursive descent parser with operator precedence:

```
parse_expr()
  └── parse_or()         /* | operator (lowest precedence) */
        └── parse_and()     /* & operator */
              └── parse_compare() /* =, !=, <, >, <=, >= */
                    └── parse_add()   /* +, - */
                          └── parse_mul()   /* *, /, % */
                                └── parse_primary() /* atoms, ( expr ), : regex */
```

### Operators

#### Arithmetic Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `expr 2 + 3` → `5` |
| `-` | Subtraction | `expr 5 - 2` → `3` |
| `*` | Multiplication | `expr 4 \* 3` → `12` |
| `/` | Integer division | `expr 10 / 3` → `3` |
| `%` | Modulo | `expr 10 % 3` → `1` |

#### Comparison Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `=` | Equal | `expr abc = abc` → `1` |
| `!=` | Not equal | `expr abc != def` → `1` |
| `<` | Less than | `expr 1 \< 2` → `1` |
| `>` | Greater than | `expr 2 \> 1` → `1` |
| `<=` | Less or equal | `expr 1 \<= 1` → `1` |
| `>=` | Greater or equal | `expr 2 \>= 1` → `1` |

Comparisons between numeric strings use numeric ordering; otherwise
locale-aware string comparison (`strcoll`) is used.

#### Logical Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `\|` | OR (short-circuit) | `expr 0 \| 5` → `5` |
| `&` | AND (short-circuit) | `expr 1 \& 2` → `1` |

#### String/Regex Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `:` | Regex match | `expr hello : 'hel\(.*\)'` → `lo` |
| `match` | Same as `:` | `expr match hello 'h.*'` |
| `substr` | Substring | `expr substr hello 2 3` → `ell` |
| `index` | Character position | `expr index hello l` → `3` |
| `length` | String length | `expr length hello` → `5` |

### Regex Matching

The `:` operator uses POSIX basic regular expressions (`regcomp` with
`REG_NOSUB` or group capture):

```c
/* expr STRING : REGEX */
/* Returns captured \(...\) group or match length */
```

If the regex contains `\(...\)`, the captured substring is returned.
Otherwise, the length of the match is returned.

### Overflow Checking

All arithmetic operations check for integer overflow:

```c
static intmax_t
safe_add(intmax_t a, intmax_t b)
{
    if ((b > 0 && a > INTMAX_MAX - b) ||
        (b < 0 && a < INTMAX_MIN - b))
        errx(2, "integer overflow");
    return a + b;
}
```

### Locale Awareness

String comparisons use `strcoll(3)` for locale-correct ordering:

```c
/* Compare as strings using locale collation */
result = strcoll(left->sval, right->sval);
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `regcomp(3)` / `regexec(3)` | Regular expression matching |
| `strcoll(3)` | Locale-aware string comparison |

## Examples

```sh
# Arithmetic
expr 2 + 3           # → 5
expr 10 / 3          # → 3
expr 7 % 4           # → 3

# String length
expr length "hello"  # → 5

# Regex match (capture group)
expr "hello-world" : 'hello-\(.*\)'  # → world

# Regex match (length)
expr "hello" : '.*'  # → 5

# Substring
expr substr "hello" 2 3  # → ell

# Index (first occurrence)
expr index "hello" "lo"  # → 3

# Comparisons
expr 42 = 42   # → 1
expr abc \< def  # → 1

# Logical OR (returns first non-zero/non-empty)
expr 0 \| 5    # → 5
expr "" \| alt # → alt

# In shell scripts
count=$(expr $count + 1)
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Expression is neither null nor zero |
| 1    | Expression is null or zero |
| 2    | Expression is invalid |
| 3    | Internal error |

## Differences from GNU expr

- No `--help` or `--version`
- Identical POSIX semantics for `:` operator
- Locale-aware string comparison by default
- Overflow results in error, not wraparound
