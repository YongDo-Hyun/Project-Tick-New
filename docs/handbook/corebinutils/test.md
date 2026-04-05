# test — Evaluate Conditional Expressions

## Overview

`test` (also invoked as `[`) evaluates file attributes, string comparisons,
and integer arithmetic, returning an exit status of 0 (true) or 1 (false).
It uses a recursive descent parser with short-circuit evaluation and
supports both POSIX and BSD extensions.

**Source**: `test/test.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
test expression
[ expression ]
```

When invoked as `[`, the last argument must be `]`.

## Source Analysis

### Parser Architecture

```c
struct parser {
    int argc;
    char **argv;
    int pos;     /* Current argument index */
};

enum token {
    TOK_OPERAND,   /* String/number operand */
    TOK_UNARY,     /* Unary operator (-f, -d, etc.) */
    TOK_BINARY,    /* Binary operator (-eq, =, etc.) */
    TOK_NOT,       /* ! */
    TOK_AND,       /* -a */
    TOK_OR,        /* -o */
    TOK_LPAREN,    /* ( */
    TOK_RPAREN,    /* ) */
    TOK_END,       /* End of arguments */
};
```

### Operator Table

```c
struct operator {
    const char *name;
    enum token type;
    int (*eval)(/* ... */);
};
```

### Recursive Descent Grammar

```
parse_expr()
  └── parse_oexpr()      /* -o (OR, lowest precedence) */
        └── parse_aexpr()    /* -a (AND) */
              └── parse_nexpr()  /* ! (NOT) */
                    └── parse_primary() /* atoms, ( expr ) */
```

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Entry: handle `[`/`test` invocation, drive parser |
| `current_arg()` | Return current argument |
| `peek_arg()` | Look at next argument |
| `advance_arg()` | Consume current argument |
| `lex_token()` | Classify current argument as token type |
| `find_operator()` | Look up operator in table |
| `parse_primary()` | Parse `( expr )`, unary ops, binary ops |
| `parse_nexpr()` | Parse `! expression` |
| `parse_aexpr()` | Parse `expr -a expr` |
| `parse_oexpr()` | Parse `expr -o expr` |
| `parse_binop()` | Evaluate binary operators |
| `evaluate_file_test()` | Evaluate file test primaries |
| `compare_integers()` | Integer comparison |
| `compare_mtime()` | File modification time comparison |
| `newer_file()` | `-nt` test |
| `older_file()` | `-ot` test |
| `same_file()` | `-ef` test |
| `parse_int()` | Parse integer with error checking |
| `effective_access()` | `eaccess(2)` or `faccessat(AT_EACCESS)` |

### File Test Primaries

| Operator | Test | System Call |
|----------|------|------------|
| `-b file` | Block special | `stat(2)` + `S_ISBLK` |
| `-c file` | Character special | `stat(2)` + `S_ISCHR` |
| `-d file` | Directory | `stat(2)` + `S_ISDIR` |
| `-e file` | Exists | `stat(2)` |
| `-f file` | Regular file | `stat(2)` + `S_ISREG` |
| `-g file` | Set-GID bit | `stat(2)` + `S_ISGID` |
| `-h file` | Symbolic link | `lstat(2)` + `S_ISLNK` |
| `-k file` | Sticky bit | `stat(2)` + `S_ISVTX` |
| `-L file` | Symbolic link | `lstat(2)` + `S_ISLNK` |
| `-p file` | Named pipe (FIFO) | `stat(2)` + `S_ISFIFO` |
| `-r file` | Readable | `eaccess(2)` or `faccessat(2)` |
| `-s file` | Non-zero size | `stat(2)` + `st_size > 0` |
| `-S file` | Socket | `stat(2)` + `S_ISSOCK` |
| `-t fd` | Is a terminal | `isatty(3)` |
| `-u file` | Set-UID bit | `stat(2)` + `S_ISUID` |
| `-w file` | Writable | `eaccess(2)` or `faccessat(2)` |
| `-x file` | Executable | `eaccess(2)` or `faccessat(2)` |
| `-O file` | Owned by EUID | `stat(2)` + `st_uid == geteuid()` |
| `-G file` | Group matches EGID | `stat(2)` + `st_gid == getegid()` |

### String Operators

| Operator | Description |
|----------|-------------|
| `-z string` | String is zero length |
| `-n string` | String is non-zero length |
| `s1 = s2` | Strings are identical |
| `s1 == s2` | Strings are identical (alias) |
| `s1 != s2` | Strings differ |
| `s1 < s2` | String less than (lexicographic) |
| `s1 > s2` | String greater than (lexicographic) |

### Integer Operators

| Operator | Description |
|----------|-------------|
| `n1 -eq n2` | Equal |
| `n1 -ne n2` | Not equal |
| `n1 -lt n2` | Less than |
| `n1 -le n2` | Less or equal |
| `n1 -gt n2` | Greater than |
| `n1 -ge n2` | Greater or equal |

### File Comparison Operators

| Operator | Description |
|----------|-------------|
| `f1 -nt f2` | f1 is newer than f2 |
| `f1 -ot f2` | f1 is older than f2 |
| `f1 -ef f2` | f1 and f2 are the same file (device + inode) |

### Short-Circuit Evaluation

```c
static int
parse_oexpr(struct parser *p)
{
    int result = parse_aexpr(p);

    while (current_is(p, "-o")) {
        advance_arg(p);
        int right = parse_aexpr(p);
        result = result || right;  /* Short-circuit */
    }

    return result;
}

static int
parse_aexpr(struct parser *p)
{
    int result = parse_nexpr(p);

    while (current_is(p, "-a")) {
        advance_arg(p);
        int right = parse_nexpr(p);
        result = result && right;  /* Short-circuit */
    }

    return result;
}
```

### Bracket Mode

```c
int main(int argc, char *argv[])
{
    /* If invoked as "[", last arg must be "]" */
    const char *progname = basename(argv[0]);
    if (strcmp(progname, "[") == 0) {
        if (argc < 2 || strcmp(argv[argc - 1], "]") != 0)
            errx(2, "missing ]");
        argc--;  /* Remove trailing ] */
    }

    if (argc <= 1)
        return 1;  /* No expression → false */

    struct parser p = { argc - 1, argv + 1, 0 };
    int result = parse_oexpr(&p);

    if (p.pos < p.argc)
        errx(2, "unexpected argument: %s", current_arg(&p));

    return !result;  /* 0 = true, 1 = false */
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `stat(2)` | File attribute tests |
| `lstat(2)` | Symlink tests (`-h`, `-L`) |
| `eaccess(2)` / `faccessat(2)` | Permission tests (`-r`, `-w`, `-x`) |
| `isatty(3)` | Terminal test (`-t`) |
| `geteuid(3)` / `getegid(3)` | Ownership tests (`-O`, `-G`) |

## Examples

```sh
# File exists
test -f /etc/passwd && echo "exists"

# Using [ syntax
[ -d /tmp ] && echo "is a directory"

# String comparison
[ "$var" = "hello" ] && echo "match"

# Integer comparison
[ "$count" -gt 10 ] && echo "more than 10"

# Combined with AND
[ -f file.txt -a -r file.txt ] && echo "readable file"

# File newer than another
[ config.new -nt config.old ] && echo "config updated"

# Negation
[ ! -e /tmp/lockfile ] && echo "no lock"

# Parenthesized expression
[ \( -f a -o -f b \) -a -r c ]
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Expression is true |
| 1    | Expression is false |
| 2    | Invalid expression (syntax error) |
