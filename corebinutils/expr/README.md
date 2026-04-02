# expr

Standalone musl-libc-based Linux port of FreeBSD `expr` for Project Tick BSD/Linux Distribution.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

## Test

```sh
gmake -f GNUmakefile test
gmake -f GNUmakefile test CC=musl-gcc
```

## Port Strategy

- The standalone layout matches sibling Linux ports such as `bin/cat`, `bin/echo`, and `bin/chmod`: local `GNUmakefile`, local shell tests, and no dependency on the top-level FreeBSD build.
- The original FreeBSD yacc grammar was translated into a local recursive-descent parser. This removes the FreeBSD yacc build dependency and keeps precedence, associativity, exit codes, and value semantics explicit in one musl-clean source file.
- Overflow handling no longer depends on signed-wrap compiler behavior. Integer `+`, `-`, `*`, `/`, and `%` use explicit checked arithmetic around `intmax_t`.
- The `:` operator stays on POSIX `regcomp(3)` / `regexec(3)` / `regfree(3)`, and string comparisons stay on locale-aware `strcoll(3)` after `setlocale(LC_ALL, "")`.

## Linux Semantics

- Supported: POSIX option parsing with `--` for a leading negative first operand.
- Supported: explicit compatibility mode via `-e`, matching FreeBSD's relaxed numeric parsing rules for optional leading `+`, leading whitespace, and empty-string-as-zero in numeric contexts.
- Supported: BRE matching for `:` using the active libc regex engine.

## Intentionally Unsupported Semantics

- FreeBSD `check_utility_compat("expr")` auto-compat probing is not implemented. Linux builds do not carry that FreeBSD userland compatibility hook.
- The legacy `EXPR_COMPAT` environment toggle is not implemented. On Linux the compatibility mode is explicit and reproducible through `-e`.
