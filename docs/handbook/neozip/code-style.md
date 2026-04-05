# Code Style

## Overview

Neozip follows conventions derived from zlib-ng and the broader zlib
ecosystem. This document describes naming patterns, macro usage,
compiler annotations, and structural conventions observed in the
codebase.

---

## Naming Conventions

### Functions

| Scope | Convention | Example |
|---|---|---|
| Public API | `PREFIX(name)` | `PREFIX(deflateInit2)` → `deflateInit2` or `zng_deflateInit2` |
| Internal | `Z_INTERNAL` linkage | `Z_INTERNAL uint32_t adler32_c(...)` |
| Exported | `Z_EXPORT` linkage | `int32_t Z_EXPORT PREFIX(deflate)(...)` |
| Architecture | `name_arch` | `adler32_avx2`, `crc32_neon` |
| Template | `name_tpl` | `longest_match_tpl`, `inflate_fast_tpl` |

### Macros

| Pattern | Usage | Example |
|---|---|---|
| `PREFIX(name)` | Public symbol namespacing | `PREFIX(inflate)` |
| `PREFIX3(name)` | Type namespacing | `PREFIX3(stream)` → `z_stream` or `zng_stream` |
| `WITH_*` | CMake build options | `WITH_AVX2`, `WITH_GZFILEOP` |
| `X86_*` / `ARM_*` | Architecture feature guards | `X86_AVX2`, `ARM_NEON` |
| `Z_*` | Public constants | `Z_OK`, `Z_DEFLATED`, `Z_DEFAULT_STRATEGY` |
| `*_H` | Include guards | `DEFLATE_H`, `INFLATE_H` |

### Types

| Type | Definition |
|---|---|
| `Pos` | `uint16_t` — hash chain position |
| `IPos` | `uint32_t` — index position |
| `z_word_t` | `uint64_t` (64-bit) or `uint32_t` (32-bit) |
| `block_state` | `enum { need_more, block_done, finish_started, finish_done }` |
| `code` | Struct: `{ bits, op, val }` for inflate Huffman tables |
| `ct_data` | Union: `{ freq/code, dad/len }` for deflate Huffman trees |

---

## Visibility Annotations

```c
#define Z_INTERNAL   // Internal linkage (hidden from shared library API)
#define Z_EXPORT     // Public API export (__attribute__((visibility("default"))))
```

All internal helper functions use `Z_INTERNAL`. All public API functions
use `Z_EXPORT`.

---

## Compiler Annotations

From `zbuild.h`:

```c
// Force inlining
#define Z_FORCEINLINE __attribute__((always_inline)) inline

// Compiler hints
#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)

// Fallthrough annotation for switch
#define Z_FALLTHROUGH __attribute__((fallthrough))

// Target-specific compilation
#define Z_TARGET(x)  __attribute__((target(x)))

// Alignment
#define ALIGNED_(n)  __attribute__((aligned(n)))

// Restrict
#define Z_RESTRICT   __restrict__
```

### Target Attributes vs. Compile Flags

Individual SIMD functions can use `Z_TARGET` instead of file-level flags:

```c
Z_TARGET("avx2")
Z_INTERNAL uint32_t adler32_avx2(uint32_t adler, const uint8_t *buf, size_t len) {
    // AVX2 intrinsics here
}
```

However, neozip typically uses per-file compile flags set via CMake
`set_property(SOURCE ... COMPILE_OPTIONS ...)`.

---

## Include Patterns

### Public Headers

```
zlib.h          — Main public API (generated from zlib.h.in)
zconf.h         — Configuration (generated)
zlib_name_mangling.h — Symbol renaming
```

### Internal Headers

```
zbuild.h        — Build macros, compiler abstractions
zutil.h         — Internal utility macros and constants
deflate.h       — deflate_state, internal types
inflate.h       — inflate_state, inflate_mode
functable.h     — Function dispatch table
cpu_features.h  — CPU detection interface
deflate_p.h     — Private deflate helpers
adler32_p.h     — Private Adler-32 helpers
crc32_braid_p.h — Private CRC-32 braided implementation
trees_emit.h    — Bit emission helpers
match_tpl.h     — Template for longest_match
insert_string_tpl.h — Template for hash insertion
inffast_tpl.h   — Template for inflate_fast
```

### Include Order

Source files typically include:
```c
#include "zbuild.h"       // Always first (defines PREFIX, types)
#include "deflate.h"      // or "inflate.h"
#include "functable.h"    // If dispatching
#include "<arch>_features.h"  // If architecture-specific
```

---

## Template Pattern

Several functions use a "template" pattern via preprocessor includes:

```c
// match_tpl.h — template for longest_match
// Defines LONGEST_MATCH, COMPARE256, INSERT_STRING via macros
// Then included by each architecture variant:

// In longest_match_sse2.c:
#define LONGEST_MATCH longest_match_sse2
#define COMPARE256    compare256_sse2
#include "match_tpl.h"
```

This avoids code duplication while allowing architecture-specific
function names and intrinsics.

---

## Struct Alignment

Performance-critical structures use `ALIGNED_(64)` for cache-line
alignment:

```c
struct ALIGNED_(64) internal_state { ... };  // deflate_state
struct ALIGNED_(64) inflate_state { ... };
```

This prevents false sharing and ensures SIMD-friendly alignment.

---

## Conditional Compilation

Architecture and feature guards follow a consistent pattern:

```c
#ifdef X86_AVX2
    if (cf.x86.has_avx2) {
        functable.adler32 = adler32_avx2;
    }
#endif
```

The `#ifdef` tests compile-time availability (was the source included
in the build?). The runtime `if` tests CPU capability.

### Build Option Guards

```c
#ifdef WITH_GZFILEOP    // Gzip file operations
#ifdef ZLIB_COMPAT      // zlib-compatible API
#ifndef Z_SOLO          // Not a standalone/embedded build
#ifdef HAVE_BUILTIN_CTZ // Compiler has __builtin_ctz
```

---

## Error Handling Style

Functions return `int` error codes from the `Z_*` set:

```c
if (strm == NULL || strm->state == NULL)
    return Z_STREAM_ERROR;
```

Internal functions that cannot fail return `void` or the computed value
directly. Error strings are set via `strm->msg`:

```c
strm->msg = (char *)"incorrect header check";
state->mode = BAD;
```

---

## Memory Management

All dynamically allocated memory goes through user-provided allocators:

```c
void *zalloc(void *opaque, unsigned items, unsigned size);
void  zfree(void *opaque, void *address);
```

If `strm->zalloc` and `strm->zfree` are `Z_NULL`, default `malloc`/`free`
wrappers are used. The single-allocation strategy (`alloc_deflate` /
`alloc_inflate`) minimises the number of allocator calls.

---

## Formatting

- **Indentation**: 4 spaces (no tabs in main source)
- **Braces**: K&R style (opening brace on same line)
- **Line length**: ~80–100 characters preferred
- **Comments**: C-style `/* */` for multi-line, `//` for inline
- **Pointer declarations**: `type *name` (space before `*`)

```c
static void pqdownheap(deflate_state *s, ct_data *tree, int k) {
    int v = s->heap[k];
    int j = k << 1;
    while (j <= s->heap_len) {
        if (j < s->heap_len &&
            SMALLER(tree, s->heap[j+1], s->heap[j], s->depth))
            j++;
        if (SMALLER(tree, v, s->heap[j], s->depth))
            break;
        s->heap[k] = s->heap[j];
        k = j;
        j <<= 1;
    }
    s->heap[k] = v;
}
```
