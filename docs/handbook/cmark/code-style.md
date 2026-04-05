# cmark — Code Style and Conventions

## Overview

This document describes the coding conventions and patterns used throughout the cmark codebase. Understanding these conventions makes the source code easier to navigate.

## Naming Conventions

### Public API Functions

All public functions use the `cmark_` prefix:
```c
cmark_node *cmark_node_new(cmark_node_type type);
cmark_parser *cmark_parser_new(int options);
char *cmark_render_html(cmark_node *root, int options);
```

### Internal (Static) Functions

File-local static functions use the `S_` prefix:
```c
static void S_render_node(cmark_node *node, cmark_event_type ev_type,
                          struct render_state *state, int options);
static cmark_node *S_node_new(cmark_node_type type, cmark_mem *mem);
static void S_free_nodes(cmark_node *e);
static bool S_is_leaf(cmark_node *node);
static int S_get_enumlevel(cmark_node *node);
```

This convention makes it immediately clear whether a function has file-local scope.

### Internal (Non-Static) Functions

Functions that are internal to the library but shared across translation units use:
- `cmark_` prefix (same as public) — declared in private headers (e.g., `parser.h`, `node.h`)
- No `S_` prefix

Examples:
```c
// In node.h (private header):
void cmark_node_set_type(cmark_node *node, cmark_node_type type);
cmark_node *make_block(cmark_mem *mem, cmark_node_type type,
                       int start_line, int start_column);
```

### Struct Members

No prefix convention — struct members use plain names:
```c
struct cmark_node {
  cmark_mem *mem;
  cmark_node *next;
  cmark_node *prev;
  cmark_node *parent;
  cmark_node *first_child;
  cmark_node *last_child;
  // ...
};
```

### Type Names

Typedefs use the `cmark_` prefix:
```c
typedef struct cmark_node cmark_node;
typedef struct cmark_parser cmark_parser;
typedef struct cmark_iter cmark_iter;
typedef int32_t bufsize_t;      // Exception: no cmark_ prefix
```

### Enum Values

Enum constants use the `CMARK_` prefix with UPPER_CASE:
```c
typedef enum {
  CMARK_NODE_NONE,
  CMARK_NODE_DOCUMENT,
  CMARK_NODE_BLOCK_QUOTE,
  // ...
} cmark_node_type;
```

### Preprocessor Macros

Macros use UPPER_CASE, sometimes with `CMARK_` prefix:
```c
#define CMARK_OPT_SOURCEPOS   (1 << 1)
#define CMARK_BUF_INIT(mem)   { mem, cmark_strbuf__initbuf, 0, 0 }
#define MAX_LINK_LABEL_LENGTH 999
#define CODE_INDENT           4
```

## Error Handling Patterns

### Allocation Failure

The default allocator (`xcalloc`, `xrealloc`) aborts on failure:
```c
static void *xcalloc(size_t nmemb, size_t size) {
  void *ptr = calloc(nmemb, size);
  if (!ptr) abort();
  return ptr;
}
```

Functions that allocate never return NULL — they either succeed or terminate. This eliminates NULL-check boilerplate throughout the codebase.

### Invalid Input

Functions that receive invalid arguments typically:
1. Return 0/false/NULL for queries
2. Do nothing for mutations
3. Never crash

Example from `node.c`:
```c
int cmark_node_set_heading_level(cmark_node *node, int level) {
  if (node == NULL || node->type != CMARK_NODE_HEADING) return 0;
  if (level < 1 || level > 6) return 0;
  node->as.heading.level = level;
  return 1;
}
```

### Return Conventions

- **0/1 for success/failure**: Setter functions return 1 on success, 0 on failure
- **NULL for not found**: Lookup functions return NULL when the item doesn't exist
- **Assertion for invariants**: Internal invariants use `assert()`:
  ```c
  assert(googled_node->type == CMARK_NODE_DOCUMENT);
  ```

## Header Guard Style

```c
#ifndef CMARK_NODE_H
#define CMARK_NODE_H
// ...
#endif
```

Guards use `CMARK_` prefix + uppercase filename + `_H`.

## Include Patterns

### Public Headers
```c
#include "cmark.h"  // Always first — provides all public types
```

### Private Headers
```c
#include "node.h"      // Internal node definitions
#include "parser.h"    // Parser internals
#include "buffer.h"    // cmark_strbuf
#include "chunk.h"     // cmark_chunk
#include "references.h" // Reference map
#include "utf8.h"      // UTF-8 utilities
#include "scanners.h"  // re2c-generated scanners
```

### System Headers
```c
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
```

## Inline Functions

The `CMARK_INLINE` macro abstracts compiler-specific inline syntax:
```c
#ifdef _MSC_VER
#define CMARK_INLINE __forceinline
#else
#define CMARK_INLINE __inline__
#endif
```

Used for small, hot-path functions in headers:
```c
static CMARK_INLINE void cmark_chunk_free(cmark_mem *mem, cmark_chunk *c) { ... }
static CMARK_INLINE cmark_chunk cmark_chunk_dup(...) { ... }
```

## Memory Ownership Patterns

### Owning vs Non-Owning

The `cmark_chunk` type makes ownership explicit:
- `alloc > 0` → the chunk owns the memory and must free it
- `alloc == 0` → the chunk borrows memory from elsewhere

### Transfer of Ownership

`cmark_strbuf_detach()` transfers ownership from a strbuf to the caller:
```c
unsigned char *data = cmark_strbuf_detach(&buf);
// Caller now owns 'data' and must free it
```

### Consistent Cleanup

Free functions null out pointers after freeing:
```c
static CMARK_INLINE void cmark_chunk_free(cmark_mem *mem, cmark_chunk *c) {
  if (c->alloc)
    mem->free((void *)c->data);
  c->data = NULL;      // NULL after free
  c->alloc = 0;
  c->len = 0;
}
```

## Iterative vs Recursive Patterns

The codebase avoids recursion for tree operations to prevent stack overflow on deeply nested input:

### Iterative Tree Destruction
`S_free_nodes()` uses sibling-list splicing instead of recursion:
```c
// Splice children into sibling chain
if (e->first_child) {
  cmark_node *last = e->last_child;
  last->next = e->next;
  e->next = e->first_child;
}
```

### Iterator-Based Traversal
All rendering uses `cmark_iter` instead of recursive `render_children()`:
```c
while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
  cur = cmark_iter_get_node(iter);
  S_render_node(cur, ev_type, &state, options);
}
```

## Type Size Definitions

```c
typedef int32_t bufsize_t;
```

Buffer sizes use `int32_t` (not `size_t`) to:
1. Allow negative values for error signaling
2. Keep node structs compact (32-bit vs 64-bit on LP64)
3. Limit maximum allocation to 2GB (adequate for text processing)

## Bitmask Patterns

Option flags use single-bit constants:
```c
#define CMARK_OPT_SOURCEPOS      (1 << 1)
#define CMARK_OPT_HARDBREAKS     (1 << 2)
#define CMARK_OPT_UNSAFE         (1 << 17)
#define CMARK_OPT_NOBREAKS       (1 << 4)
#define CMARK_OPT_VALIDATE_UTF8  (1 << 9)
#define CMARK_OPT_SMART          (1 << 10)
```

Tested with bitwise AND:
```c
if (options & CMARK_OPT_SOURCEPOS) { ... }
```

Combined with bitwise OR:
```c
int options = CMARK_OPT_SOURCEPOS | CMARK_OPT_SMART;
```

## Leaf Mask Pattern

`S_is_leaf()` in `iterator.c` uses a bitmask for O(1) node-type classification:
```c
static const int S_leaf_mask =
    (1 << CMARK_NODE_HTML_BLOCK) | (1 << CMARK_NODE_THEMATIC_BREAK) |
    (1 << CMARK_NODE_CODE_BLOCK) | (1 << CMARK_NODE_TEXT) | ...;

static bool S_is_leaf(cmark_node *node) {
  return ((1 << node->type) & S_leaf_mask) != 0;
}
```

This is more efficient than a switch statement for a simple boolean classification.

## Cross-References

- [architecture.md](architecture.md) — Design decisions
- [memory-management.md](memory-management.md) — Allocator patterns
- [public-api.md](public-api.md) — Public API naming
