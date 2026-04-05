# cmark — Public API Reference

## Header: `cmark.h`

All public API functions, types, and constants are declared in `cmark.h`. Functions marked with `CMARK_EXPORT` are exported from the shared library. The header is usable from C++ via `extern "C"` guards.

---

## Type Definitions

### Node Types

```c
typedef enum {
  /* Error status */
  CMARK_NODE_NONE,

  /* Block nodes */
  CMARK_NODE_DOCUMENT,
  CMARK_NODE_BLOCK_QUOTE,
  CMARK_NODE_LIST,
  CMARK_NODE_ITEM,
  CMARK_NODE_CODE_BLOCK,
  CMARK_NODE_HTML_BLOCK,
  CMARK_NODE_CUSTOM_BLOCK,
  CMARK_NODE_PARAGRAPH,
  CMARK_NODE_HEADING,
  CMARK_NODE_THEMATIC_BREAK,

  /* Range sentinels */
  CMARK_NODE_FIRST_BLOCK = CMARK_NODE_DOCUMENT,
  CMARK_NODE_LAST_BLOCK  = CMARK_NODE_THEMATIC_BREAK,

  /* Inline nodes */
  CMARK_NODE_TEXT,
  CMARK_NODE_SOFTBREAK,
  CMARK_NODE_LINEBREAK,
  CMARK_NODE_CODE,
  CMARK_NODE_HTML_INLINE,
  CMARK_NODE_CUSTOM_INLINE,
  CMARK_NODE_EMPH,
  CMARK_NODE_STRONG,
  CMARK_NODE_LINK,
  CMARK_NODE_IMAGE,

  CMARK_NODE_FIRST_INLINE = CMARK_NODE_TEXT,
  CMARK_NODE_LAST_INLINE  = CMARK_NODE_IMAGE
} cmark_node_type;
```

### List Types

```c
typedef enum {
  CMARK_NO_LIST,
  CMARK_BULLET_LIST,
  CMARK_ORDERED_LIST
} cmark_list_type;
```

### Delimiter Types

```c
typedef enum {
  CMARK_NO_DELIM,
  CMARK_PERIOD_DELIM,
  CMARK_PAREN_DELIM
} cmark_delim_type;
```

### Event Types (for iterator)

```c
typedef enum {
  CMARK_EVENT_NONE,
  CMARK_EVENT_DONE,
  CMARK_EVENT_ENTER,
  CMARK_EVENT_EXIT
} cmark_event_type;
```

### Opaque Types

```c
typedef struct cmark_node   cmark_node;
typedef struct cmark_parser cmark_parser;
typedef struct cmark_iter   cmark_iter;
```

### Memory Allocator

```c
typedef struct cmark_mem {
  void *(*calloc)(size_t, size_t);
  void *(*realloc)(void *, size_t);
  void (*free)(void *);
} cmark_mem;
```

---

## Simple Interface

### `cmark_markdown_to_html`

```c
CMARK_EXPORT
char *cmark_markdown_to_html(const char *text, size_t len, int options);
```

Converts CommonMark text to HTML in a single call. The input `text` must be UTF-8 encoded. The returned string is null-terminated and allocated via the default allocator; the caller must free it with `free()`.

**Implementation** (in `cmark.c`): Calls `cmark_parse_document()`, then `cmark_render_html()`, then `cmark_node_free()`.

---

## Node Classification

### `cmark_node_is_block`

```c
CMARK_EXPORT bool cmark_node_is_block(cmark_node *node);
```

Returns `true` if `node->type` is between `CMARK_NODE_FIRST_BLOCK` and `CMARK_NODE_LAST_BLOCK` inclusive. Returns `false` for NULL.

### `cmark_node_is_inline`

```c
CMARK_EXPORT bool cmark_node_is_inline(cmark_node *node);
```

Returns `true` if `node->type` is between `CMARK_NODE_FIRST_INLINE` and `CMARK_NODE_LAST_INLINE` inclusive. Returns `false` for NULL.

### `cmark_node_is_leaf`

```c
CMARK_EXPORT bool cmark_node_is_leaf(cmark_node *node);
```

Returns `true` for node types that cannot have children:
- `CMARK_NODE_THEMATIC_BREAK`
- `CMARK_NODE_CODE_BLOCK`
- `CMARK_NODE_TEXT`
- `CMARK_NODE_SOFTBREAK`
- `CMARK_NODE_LINEBREAK`
- `CMARK_NODE_CODE`
- `CMARK_NODE_HTML_INLINE`

Note: `CMARK_NODE_HTML_BLOCK` is **not** classified as a leaf by `cmark_node_is_leaf()`, though the iterator treats it as one (see `S_leaf_mask` in `iterator.c`).

---

## Node Creation and Destruction

### `cmark_node_new`

```c
CMARK_EXPORT cmark_node *cmark_node_new(cmark_node_type type);
```

Creates a new node of the given type using the default memory allocator. For `CMARK_NODE_HEADING`, the level defaults to 1. For `CMARK_NODE_LIST`, the list type defaults to `CMARK_BULLET_LIST` with `start = 0` and `tight = false`.

### `cmark_node_new_with_mem`

```c
CMARK_EXPORT cmark_node *cmark_node_new_with_mem(cmark_node_type type, cmark_mem *mem);
```

Same as `cmark_node_new` but uses the specified memory allocator. All nodes in a single tree must use the same allocator.

### `cmark_node_free`

```c
CMARK_EXPORT void cmark_node_free(cmark_node *node);
```

Frees the node and all its descendants. The node is first unlinked from its siblings/parent. The internal `S_free_nodes()` function iterates the subtree (splicing children into a flat list for iterative freeing) and releases type-specific memory:
- `CMARK_NODE_CODE_BLOCK`: frees `data` and `as.code.info`
- `CMARK_NODE_TEXT`, `CMARK_NODE_HTML_INLINE`, `CMARK_NODE_CODE`, `CMARK_NODE_HTML_BLOCK`: frees `data`
- `CMARK_NODE_LINK`, `CMARK_NODE_IMAGE`: frees `as.link.url` and `as.link.title`
- `CMARK_NODE_CUSTOM_BLOCK`, `CMARK_NODE_CUSTOM_INLINE`: frees `as.custom.on_enter` and `as.custom.on_exit`

---

## Tree Traversal

### `cmark_node_next`

```c
CMARK_EXPORT cmark_node *cmark_node_next(cmark_node *node);
```

Returns the next sibling, or NULL.

### `cmark_node_previous`

```c
CMARK_EXPORT cmark_node *cmark_node_previous(cmark_node *node);
```

Returns the previous sibling, or NULL.

### `cmark_node_parent`

```c
CMARK_EXPORT cmark_node *cmark_node_parent(cmark_node *node);
```

Returns the parent node, or NULL.

### `cmark_node_first_child`

```c
CMARK_EXPORT cmark_node *cmark_node_first_child(cmark_node *node);
```

Returns the first child, or NULL.

### `cmark_node_last_child`

```c
CMARK_EXPORT cmark_node *cmark_node_last_child(cmark_node *node);
```

Returns the last child, or NULL.

---

## Iterator API

### `cmark_iter_new`

```c
CMARK_EXPORT cmark_iter *cmark_iter_new(cmark_node *root);
```

Creates a new iterator starting at `root`. Returns NULL if `root` is NULL. The iterator begins in a pre-first state (`CMARK_EVENT_NONE`); the first call to `cmark_iter_next()` returns `CMARK_EVENT_ENTER` for the root.

### `cmark_iter_free`

```c
CMARK_EXPORT void cmark_iter_free(cmark_iter *iter);
```

Frees the iterator. Does not free any nodes.

### `cmark_iter_next`

```c
CMARK_EXPORT cmark_event_type cmark_iter_next(cmark_iter *iter);
```

Advances to the next node and returns the event type:
- `CMARK_EVENT_ENTER` — entering a node (for non-leaf nodes, children follow)
- `CMARK_EVENT_EXIT` — leaving a node (all children have been visited)
- `CMARK_EVENT_DONE` — iteration complete (returned to root)

Leaf nodes only generate `ENTER` events, never `EXIT`.

### `cmark_iter_get_node`

```c
CMARK_EXPORT cmark_node *cmark_iter_get_node(cmark_iter *iter);
```

Returns the current node.

### `cmark_iter_get_event_type`

```c
CMARK_EXPORT cmark_event_type cmark_iter_get_event_type(cmark_iter *iter);
```

Returns the current event type.

### `cmark_iter_get_root`

```c
CMARK_EXPORT cmark_node *cmark_iter_get_root(cmark_iter *iter);
```

Returns the root node of the iteration.

### `cmark_iter_reset`

```c
CMARK_EXPORT void cmark_iter_reset(cmark_iter *iter, cmark_node *current,
                                    cmark_event_type event_type);
```

Resets the iterator position. The node must be a descendant of the root (or the root itself).

---

## Node Accessors

### User Data

```c
CMARK_EXPORT void *cmark_node_get_user_data(cmark_node *node);
CMARK_EXPORT int   cmark_node_set_user_data(cmark_node *node, void *user_data);
```

Get/set arbitrary user data pointer. Returns 0 on failure, 1 on success. cmark does not manage the lifecycle of user data.

### Type Information

```c
CMARK_EXPORT cmark_node_type cmark_node_get_type(cmark_node *node);
CMARK_EXPORT const char     *cmark_node_get_type_string(cmark_node *node);
```

`cmark_node_get_type_string()` returns strings like `"document"`, `"paragraph"`, `"heading"`, `"text"`, `"emph"`, `"strong"`, `"link"`, `"image"`, etc. Returns `"<unknown>"` for unrecognized types.

### String Content

```c
CMARK_EXPORT const char *cmark_node_get_literal(cmark_node *node);
CMARK_EXPORT int         cmark_node_set_literal(cmark_node *node, const char *content);
```

Works for `CMARK_NODE_HTML_BLOCK`, `CMARK_NODE_TEXT`, `CMARK_NODE_HTML_INLINE`, `CMARK_NODE_CODE`, and `CMARK_NODE_CODE_BLOCK`. Returns NULL / 0 for other types.

### Heading Level

```c
CMARK_EXPORT int cmark_node_get_heading_level(cmark_node *node);
CMARK_EXPORT int cmark_node_set_heading_level(cmark_node *node, int level);
```

Only works for `CMARK_NODE_HEADING`. Level must be 1–6. Returns 0 on error.

### List Properties

```c
CMARK_EXPORT cmark_list_type  cmark_node_get_list_type(cmark_node *node);
CMARK_EXPORT int              cmark_node_set_list_type(cmark_node *node, cmark_list_type type);
CMARK_EXPORT cmark_delim_type cmark_node_get_list_delim(cmark_node *node);
CMARK_EXPORT int              cmark_node_set_list_delim(cmark_node *node, cmark_delim_type delim);
CMARK_EXPORT int              cmark_node_get_list_start(cmark_node *node);
CMARK_EXPORT int              cmark_node_set_list_start(cmark_node *node, int start);
CMARK_EXPORT int              cmark_node_get_list_tight(cmark_node *node);
CMARK_EXPORT int              cmark_node_set_list_tight(cmark_node *node, int tight);
```

All list accessors only work for `CMARK_NODE_LIST`. `set_list_start` rejects negative values. `set_list_tight` interprets `tight == 1` as true.

### Code Block Info

```c
CMARK_EXPORT const char *cmark_node_get_fence_info(cmark_node *node);
CMARK_EXPORT int         cmark_node_set_fence_info(cmark_node *node, const char *info);
```

The info string from a fenced code block (e.g., `"python"` from ` ```python `). Only works for `CMARK_NODE_CODE_BLOCK`.

### Link/Image Properties

```c
CMARK_EXPORT const char *cmark_node_get_url(cmark_node *node);
CMARK_EXPORT int         cmark_node_set_url(cmark_node *node, const char *url);
CMARK_EXPORT const char *cmark_node_get_title(cmark_node *node);
CMARK_EXPORT int         cmark_node_set_title(cmark_node *node, const char *title);
```

Only work for `CMARK_NODE_LINK` and `CMARK_NODE_IMAGE`. Return NULL / 0 for other types.

### Custom Block/Inline

```c
CMARK_EXPORT const char *cmark_node_get_on_enter(cmark_node *node);
CMARK_EXPORT int         cmark_node_set_on_enter(cmark_node *node, const char *on_enter);
CMARK_EXPORT const char *cmark_node_get_on_exit(cmark_node *node);
CMARK_EXPORT int         cmark_node_set_on_exit(cmark_node *node, const char *on_exit);
```

Only work for `CMARK_NODE_CUSTOM_BLOCK` and `CMARK_NODE_CUSTOM_INLINE`.

### Source Position

```c
CMARK_EXPORT int cmark_node_get_start_line(cmark_node *node);
CMARK_EXPORT int cmark_node_get_start_column(cmark_node *node);
CMARK_EXPORT int cmark_node_get_end_line(cmark_node *node);
CMARK_EXPORT int cmark_node_get_end_column(cmark_node *node);
```

Line and column numbers are 1-based. These are populated during parsing if `CMARK_OPT_SOURCEPOS` is set.

---

## Tree Manipulation

### `cmark_node_unlink`

```c
CMARK_EXPORT void cmark_node_unlink(cmark_node *node);
```

Removes `node` from the tree (detaching from parent and siblings) without freeing its memory.

### `cmark_node_insert_before`

```c
CMARK_EXPORT int cmark_node_insert_before(cmark_node *node, cmark_node *sibling);
```

Inserts `sibling` before `node`. Validates that the parent can contain the sibling (via `S_can_contain()`). Returns 1 on success, 0 on failure.

### `cmark_node_insert_after`

```c
CMARK_EXPORT int cmark_node_insert_after(cmark_node *node, cmark_node *sibling);
```

Inserts `sibling` after `node`. Returns 1 on success, 0 on failure.

### `cmark_node_replace`

```c
CMARK_EXPORT int cmark_node_replace(cmark_node *oldnode, cmark_node *newnode);
```

Replaces `oldnode` with `newnode` in the tree. The old node is unlinked but not freed.

### `cmark_node_prepend_child`

```c
CMARK_EXPORT int cmark_node_prepend_child(cmark_node *node, cmark_node *child);
```

Adds `child` as the first child of `node`. Validates containership.

### `cmark_node_append_child`

```c
CMARK_EXPORT int cmark_node_append_child(cmark_node *node, cmark_node *child);
```

Adds `child` as the last child of `node`. Validates containership.

### `cmark_consolidate_text_nodes`

```c
CMARK_EXPORT void cmark_consolidate_text_nodes(cmark_node *root);
```

Merges adjacent `CMARK_NODE_TEXT` children into single text nodes throughout the subtree. Uses an iterator to find consecutive text nodes and concatenates their data via `cmark_strbuf`.

---

## Parsing Functions

### `cmark_parser_new`

```c
CMARK_EXPORT cmark_parser *cmark_parser_new(int options);
```

Creates a parser with the default memory allocator and a new document root.

### `cmark_parser_new_with_mem`

```c
CMARK_EXPORT cmark_parser *cmark_parser_new_with_mem(int options, cmark_mem *mem);
```

Creates a parser with the specified allocator.

### `cmark_parser_new_with_mem_into_root`

```c
CMARK_EXPORT cmark_parser *cmark_parser_new_with_mem_into_root(
    int options, cmark_mem *mem, cmark_node *root);
```

Creates a parser that appends parsed content to an existing root node. Useful for assembling a single document from multiple parsed fragments.

### `cmark_parser_free`

```c
CMARK_EXPORT void cmark_parser_free(cmark_parser *parser);
```

Frees the parser and its internal buffers. Does NOT free the parsed document tree.

### `cmark_parser_feed`

```c
CMARK_EXPORT void cmark_parser_feed(cmark_parser *parser, const char *buffer, size_t len);
```

Feeds a chunk of input data to the parser. Can be called multiple times for streaming input.

### `cmark_parser_finish`

```c
CMARK_EXPORT cmark_node *cmark_parser_finish(cmark_parser *parser);
```

Finalizes parsing and returns the document root. Must be called after all input has been fed. Triggers `finalize_document()` which closes all open blocks and runs inline parsing.

### `cmark_parse_document`

```c
CMARK_EXPORT cmark_node *cmark_parse_document(const char *buffer, size_t len, int options);
```

Convenience function equivalent to: create parser → feed entire buffer → finish → free parser. Returns the document root.

### `cmark_parse_file`

```c
CMARK_EXPORT cmark_node *cmark_parse_file(FILE *f, int options);
```

Reads from a `FILE*` in 4096-byte chunks and parses incrementally.

---

## Rendering Functions

### `cmark_render_html`

```c
CMARK_EXPORT char *cmark_render_html(cmark_node *root, int options);
```

Renders to HTML. Caller must free returned string.

### `cmark_render_xml`

```c
CMARK_EXPORT char *cmark_render_xml(cmark_node *root, int options);
```

Renders to XML with CommonMark DTD. Includes `<?xml version="1.0" encoding="UTF-8"?>` header.

### `cmark_render_man`

```c
CMARK_EXPORT char *cmark_render_man(cmark_node *root, int options, int width);
```

Renders to groff man page format. `width` controls line wrapping (0 = no wrap).

### `cmark_render_commonmark`

```c
CMARK_EXPORT char *cmark_render_commonmark(cmark_node *root, int options, int width);
```

Renders back to CommonMark format. `width` controls line wrapping.

### `cmark_render_latex`

```c
CMARK_EXPORT char *cmark_render_latex(cmark_node *root, int options, int width);
```

Renders to LaTeX. `width` controls line wrapping.

---

## Option Constants

### Rendering Options

```c
#define CMARK_OPT_DEFAULT     0         // No special options
#define CMARK_OPT_SOURCEPOS   (1 << 1)  // data-sourcepos attributes (HTML), sourcepos attributes (XML)
#define CMARK_OPT_HARDBREAKS  (1 << 2)  // Render softbreaks as <br /> or \\
#define CMARK_OPT_SAFE        (1 << 3)  // Legacy — safe mode is now default
#define CMARK_OPT_UNSAFE      (1 << 17) // Render raw HTML and dangerous URLs
#define CMARK_OPT_NOBREAKS    (1 << 4)  // Render softbreaks as spaces
```

### Parsing Options

```c
#define CMARK_OPT_NORMALIZE     (1 << 8)  // Legacy — no effect
#define CMARK_OPT_VALIDATE_UTF8 (1 << 9)  // Replace invalid UTF-8 with U+FFFD
#define CMARK_OPT_SMART         (1 << 10) // Smart quotes and dashes
```

---

## Memory Allocator

### `cmark_get_default_mem_allocator`

```c
CMARK_EXPORT cmark_mem *cmark_get_default_mem_allocator(void);
```

Returns a pointer to the default allocator (`DEFAULT_MEM_ALLOCATOR` in `cmark.c`) which wraps `calloc`, `realloc`, and `free` with abort-on-failure guards.

---

## Version API

### `cmark_version`

```c
CMARK_EXPORT int cmark_version(void);
```

Returns the version as a packed integer: `(major << 16) | (minor << 8) | patch`.

### `cmark_version_string`

```c
CMARK_EXPORT const char *cmark_version_string(void);
```

Returns the version as a human-readable string (e.g., `"0.31.2"`).

---

## Node Integrity Checking

```c
CMARK_EXPORT int cmark_node_check(cmark_node *node, FILE *out);
```

Validates the structural integrity of the node tree, printing errors to `out`. Returns the number of errors found. Available in all builds but primarily useful in debug builds.

---

## Cross-References

- [ast-node-system.md](ast-node-system.md) — Internal struct definitions behind these opaque types
- [iterator-system.md](iterator-system.md) — Detailed iterator mechanics
- [memory-management.md](memory-management.md) — Allocator details and buffer management
- [block-parsing.md](block-parsing.md) — How `cmark_parser_feed` and `cmark_parser_finish` work internally
- [html-renderer.md](html-renderer.md) — How `cmark_render_html` generates output
