# cmark — Memory Management

## Overview

cmark's memory management is built around three concepts:
1. **Pluggable allocator** (`cmark_mem`) — a function-pointer table for calloc/realloc/free
2. **Owning buffer** (`cmark_strbuf`) — a growable byte buffer that owns its memory
3. **Non-owning slice** (`cmark_chunk`) — a view into either a `cmark_strbuf` or external memory

## Pluggable Allocator

### `cmark_mem` Structure

```c
typedef struct cmark_mem {
  void *(*calloc)(size_t, size_t);
  void *(*realloc)(void *, size_t);
  void (*free)(void *);
} cmark_mem;
```

All allocation throughout cmark respects this interface. Every node, buffer, parser, and iterator receives a `cmark_mem *` and uses it for all allocations.

### Default Allocator

```c
static void *xcalloc(size_t nmemb, size_t size) {
  void *ptr = calloc(nmemb, size);
  if (!ptr) abort();
  return ptr;
}

static void *xrealloc(void *ptr, size_t size) {
  void *new_ptr = realloc(ptr, size);
  if (!new_ptr) abort();
  return new_ptr;
}

cmark_mem DEFAULT_MEM_ALLOCATOR = {xcalloc, xrealloc, free};
```

The default allocator wraps standard `calloc`/`realloc`/`free`, adding `abort()` on allocation failure. This means cmark never returns NULL from allocations — it terminates on out-of-memory.

### Getting the Default Allocator

```c
cmark_mem *cmark_get_default_mem_allocator(void) {
  return &DEFAULT_MEM_ALLOCATOR;
}
```

### Custom Allocator Usage

Users can provide custom allocators (arena allocators, debug allocators, etc.) via:

```c
cmark_parser *cmark_parser_new_with_mem(int options, cmark_mem *mem);
cmark_node *cmark_node_new_with_mem(cmark_node_type type, cmark_mem *mem);
```

The allocator propagates: nodes created by the parser inherit the parser's allocator. Iterators use the root node's allocator.

## Growable Buffer (`cmark_strbuf`)

### Structure

```c
struct cmark_strbuf {
  cmark_mem *mem;
  unsigned char *ptr;
  bufsize_t asize;   // allocated size
  bufsize_t size;     // used size (excluding NUL terminator)
};
```

### Initialization

```c
#define CMARK_BUF_INIT(mem)  { mem, cmark_strbuf__initbuf, 0, 0 }
```

`cmark_strbuf__initbuf` is a static empty buffer that avoids allocating for empty strings:
```c
unsigned char cmark_strbuf__initbuf[1] = {0};
```

This means: uninitialized/empty buffers point to a shared static empty string rather than NULL. This eliminates NULL checks throughout the code.

### Growth Strategy

```c
void cmark_strbuf_grow(cmark_strbuf *buf, bufsize_t target_size) {
  // Minimum allocation of 8 bytes
  bufsize_t new_size = 8;
  // Double until >= target (or use 2x current if growing existing)
  if (buf->asize) {
    new_size = buf->asize;
  }
  while (new_size < target_size) {
    new_size *= 2;
  }
  // Allocate
  if (buf->ptr == cmark_strbuf__initbuf) {
    buf->ptr = (unsigned char *)buf->mem->calloc(new_size, 1);
  } else {
    buf->ptr = (unsigned char *)buf->mem->realloc(buf->ptr, new_size);
  }
  buf->asize = new_size;
}
```

The growth strategy doubles the capacity each time, ensuring amortized O(1) appends. Minimum capacity is 8 bytes.

When the buffer transitions from the shared static init to a real allocation, `calloc` is used (zero-initialized). Subsequent growths use `realloc`.

### Key Operations

```c
// Appending
void cmark_strbuf_put(cmark_strbuf *buf, const unsigned char *data, bufsize_t len);
void cmark_strbuf_puts(cmark_strbuf *buf, const char *string);
void cmark_strbuf_putc(cmark_strbuf *buf, int c);

// Printf-style
void cmark_strbuf_printf(cmark_strbuf *buf, const char *fmt, ...);
void cmark_strbuf_vprintf(cmark_strbuf *buf, const char *fmt, va_list ap);

// Manipulation
void cmark_strbuf_clear(cmark_strbuf *buf);     // Reset size to 0, keep allocation
void cmark_strbuf_set(cmark_strbuf *buf, const unsigned char *data, bufsize_t len);
void cmark_strbuf_sets(cmark_strbuf *buf, const char *string);
void cmark_strbuf_copy_cstr(char *data, bufsize_t datasize, const cmark_strbuf *buf);
void cmark_strbuf_swap(cmark_strbuf *a, cmark_strbuf *b);

// Whitespace
void cmark_strbuf_trim(cmark_strbuf *buf);       // Trim leading and trailing whitespace
void cmark_strbuf_normalize_whitespace(cmark_strbuf *buf);  // Collapse runs to single space
void cmark_strbuf_unescape(cmark_strbuf *buf);   // Process backslash escapes

// Lifecycle
unsigned char *cmark_strbuf_detach(cmark_strbuf *buf);  // Return ptr, reset buf to init
void cmark_strbuf_free(cmark_strbuf *buf);               // Free memory, reset to init
```

### `cmark_strbuf_detach()`

```c
unsigned char *cmark_strbuf_detach(cmark_strbuf *buf) {
  unsigned char *data = buf->ptr;
  if (buf->asize == 0) {
    // Never allocated — return a new empty string
    data = (unsigned char *)buf->mem->calloc(1, 1);
  }
  // Reset buffer to initial state
  buf->ptr = cmark_strbuf__initbuf;
  buf->asize = 0;
  buf->size = 0;
  return data;
}
```

Transfers ownership of the buffer's memory to the caller. The buffer is reset to the empty init state. The caller must `free()` the returned pointer.

### Whitespace Normalization

```c
void cmark_strbuf_normalize_whitespace(cmark_strbuf *s) {
  bool last_char_was_space = false;
  bufsize_t r, w;
  for (r = 0, w = 0; r < s->size; r++) {
    if (cmark_isspace(s->ptr[r])) {
      if (!last_char_was_space) {
        s->ptr[w++] = ' ';
        last_char_was_space = true;
      }
    } else {
      s->ptr[w++] = s->ptr[r];
      last_char_was_space = false;
    }
  }
  cmark_strbuf_truncate(s, w);
}
```

Collapses consecutive whitespace into a single space. Uses an in-place read/write cursor technique.

### Backslash Unescape

```c
void cmark_strbuf_unescape(cmark_strbuf *buf) {
  bufsize_t r, w;
  for (r = 0, w = 0; r < buf->size; r++) {
    if (buf->ptr[r] == '\\' && cmark_ispunct(buf->ptr[r + 1]))
      r++;
    buf->ptr[w++] = buf->ptr[r];
  }
  cmark_strbuf_truncate(buf, w);
}
```

Removes backslash escapes before punctuation characters, in-place.

## Non-Owning Slice (`cmark_chunk`)

### Structure

```c
typedef struct {
  const unsigned char *data;
  bufsize_t len;
  bufsize_t alloc;  // 0 if non-owning, > 0 if owning
} cmark_chunk;
```

A `cmark_chunk` is either:
- **Non-owning** (`alloc == 0`): Points into someone else's memory (e.g., the parser's input buffer)
- **Owning** (`alloc > 0`): Owns its `data` pointer and must free it

### Key Operations

```c
// Create a non-owning reference
static CMARK_INLINE cmark_chunk cmark_chunk_buf_detach(cmark_strbuf *buf);
static CMARK_INLINE cmark_chunk cmark_chunk_literal(const char *data);
static CMARK_INLINE cmark_chunk cmark_chunk_dup(const cmark_chunk *ch, bufsize_t pos, bufsize_t len);

// Free (only if owning)
static CMARK_INLINE void cmark_chunk_free(cmark_mem *mem, cmark_chunk *c) {
  if (c->alloc)
    mem->free((void *)c->data);
  c->data = NULL;
  c->alloc = 0;
  c->len = 0;
}
```

### Ownership Transfer

`cmark_chunk_buf_detach()` takes ownership of a `cmark_strbuf`'s memory:

```c
static CMARK_INLINE cmark_chunk cmark_chunk_buf_detach(cmark_strbuf *buf) {
  cmark_chunk c;
  c.len = buf->size;
  c.data = cmark_strbuf_detach(buf);
  c.alloc = 1;  // Now owns the data
  return c;
}
```

### Non-Owning References

`cmark_chunk_dup()` creates a non-owning view into existing memory:

```c
static CMARK_INLINE cmark_chunk cmark_chunk_dup(const cmark_chunk *ch,
                                                 bufsize_t pos, bufsize_t len) {
  cmark_chunk c = {ch->data + pos, len, 0};  // alloc = 0: non-owning
  return c;
}
```

This is used extensively during parsing to avoid copying strings. For example, text node content during inline parsing initially points into the parser's line buffer. Only when the node outlives the parse does the data need to be copied.

## Node Memory Management

### Node Allocation

```c
static cmark_node *S_node_new(cmark_node_type type, cmark_mem *mem) {
  cmark_node *node = (cmark_node *)mem->calloc(1, sizeof(*node));
  cmark_strbuf_init(mem, &node->content, 0);
  node->type = (uint16_t)type;
  node->mem = mem;
  return node;
}
```

Nodes are zero-initialized via `calloc`. The `mem` pointer is stored on the node for later freeing.

### Node Deallocation

```c
static void S_free_nodes(cmark_node *e) {
  cmark_node *next;
  while (e != NULL) {
    // Free type-specific data
    switch (e->type) {
    case CMARK_NODE_CODE_BLOCK:
      cmark_chunk_free(e->mem, &e->as.code.info);
      cmark_chunk_free(e->mem, &e->as.literal);
      break;
    case CMARK_NODE_LINK:
    case CMARK_NODE_IMAGE:
      e->mem->free(e->as.link.url);
      e->mem->free(e->as.link.title);
      break;
    // ... other types
    }
    // Splice children into the free list
    if (e->first_child) {
      cmark_node *last = e->last_child;
      last->next = e->next;
      e->next = e->first_child;
    }
    // Advance and free
    next = e->next;
    e->mem->free(e);
    e = next;
  }
}
```

This is an iterative (non-recursive) destructor that avoids stack overflow on deeply nested ASTs. The key technique is **sibling-list splicing**: children are inserted into the sibling chain before the current position, converting tree traversal into linear list traversal.

### What Gets Freed Per Node Type

| Node Type | Freed Data |
|-----------|-----------|
| `CODE_BLOCK` | `as.code.info` chunk, `as.literal` chunk |
| `TEXT`, `HTML_BLOCK`, `HTML_INLINE`, `CODE` | `as.literal` chunk |
| `LINK`, `IMAGE` | `as.link.url`, `as.link.title` |
| `CUSTOM_BLOCK`, `CUSTOM_INLINE` | `as.custom.on_enter`, `as.custom.on_exit` |
| `HEADING` | `as.heading.setext_content` (if chunk) |
| All nodes | `content` strbuf |

## Parser Memory

The parser allocates:
- A `cmark_parser` struct
- A `cmark_strbuf` for the current line (`linebuf`)
- A `cmark_strbuf` for collected content (`content`)
- A `cmark_reference_map` for link references
- Individual `cmark_node` objects for the AST

When `cmark_parser_free()` is called, only the parser's own resources are freed — the AST is NOT freed (the user owns it). To free the AST, call `cmark_node_free()` on the root.

## Memory Safety Patterns

1. **No NULL returns**: The default allocator aborts on failure. User allocators should do the same or handle errors externally.
2. **Init buffers**: `cmark_strbuf__initbuf` prevents NULL pointer dereferences on empty buffers.
3. **Owning vs non-owning**: The `cmark_chunk.alloc` field prevents double-frees and ensures non-owning references are not freed.
4. **Iterative destruction**: `S_free_nodes()` avoids stack overflow on deep trees.

## Cross-References

- [buffer.c](../../cmark/src/buffer.c), [buffer.h](../../cmark/src/buffer.h) — `cmark_strbuf` implementation
- [chunk.h](../../cmark/src/chunk.h) — `cmark_chunk` definition
- [cmark.c](../../cmark/src/cmark.c) — Default allocator, `cmark_get_default_mem_allocator()`
- [node.c](../../cmark/src/node.c) — Node allocation and deallocation
- [ast-node-system.md](ast-node-system.md) — Node structure and lifecycle
