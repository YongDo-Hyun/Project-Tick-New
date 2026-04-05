# cmark — AST Node System

## Overview

The AST (Abstract Syntax Tree) node system is defined across `node.h` (internal struct definitions) and `node.c` (node creation, destruction, accessor functions, and tree manipulation). Every element in a parsed CommonMark document is represented as a `cmark_node`. Nodes form a tree via parent/child/sibling pointers, with type-specific data stored in a discriminated union.

## The `cmark_node` Struct

Defined in `node.h`, this is the central data structure of the entire library:

```c
struct cmark_node {
  cmark_mem *mem;                   // Memory allocator used for this node

  struct cmark_node *next;          // Next sibling
  struct cmark_node *prev;          // Previous sibling
  struct cmark_node *parent;        // Parent node
  struct cmark_node *first_child;   // First child
  struct cmark_node *last_child;    // Last child

  void *user_data;                  // Arbitrary user-attached data

  unsigned char *data;              // String content (for text, code, HTML)
  bufsize_t len;                    // Length of data

  int start_line;                   // Source position: starting line (1-based)
  int start_column;                 // Source position: starting column (1-based)
  int end_line;                     // Source position: ending line
  int end_column;                   // Source position: ending column
  uint16_t type;                    // Node type (cmark_node_type enum value)
  uint16_t flags;                   // Internal flags (open, last-line-blank, etc.)

  union {
    cmark_list list;                // List-specific data
    cmark_code code;                // Code block-specific data
    cmark_heading heading;          // Heading-specific data
    cmark_link link;                // Link/image-specific data
    cmark_custom custom;            // Custom block/inline data
    int html_block_type;            // HTML block type (1-7)
  } as;
};
```

The union `as` means each node only occupies memory for one type-specific payload, keeping the struct compact. The largest union member determines the node's size.

## Type-Specific Structs

### `cmark_list` — List Properties

```c
typedef struct {
  int marker_offset;        // Indentation of list marker from left margin
  int padding;              // Total indentation (marker + content offset)
  int start;                // Starting number for ordered lists (0 for bullet)
  unsigned char list_type;  // CMARK_BULLET_LIST or CMARK_ORDERED_LIST
  unsigned char delimiter;  // CMARK_PERIOD_DELIM, CMARK_PAREN_DELIM, or CMARK_NO_DELIM
  unsigned char bullet_char;// '*', '-', or '+' for bullet lists
  bool tight;               // Whether the list is tight (no blank lines between items)
} cmark_list;
```

`marker_offset` and `padding` are used during block parsing to track indentation levels for list continuation. The `tight` flag is determined during block finalization by checking whether blank lines appear between list items or their children.

### `cmark_code` — Code Block Properties

```c
typedef struct {
  unsigned char *info;      // Info string (language hint, e.g., "python")
  uint8_t fence_length;     // Length of opening fence (3+ backticks or tildes)
  uint8_t fence_offset;     // Indentation of fence from left margin
  unsigned char fence_char; // '`' or '~'
  int8_t fenced;            // Whether this is a fenced code block (vs. indented)
} cmark_code;
```

For indented code blocks, `fenced` is 0, and `info`, `fence_length`, `fence_char`, and `fence_offset` are unused. For fenced code blocks, `info` is extracted from the first line of the opening fence and stored as a separately allocated string.

### `cmark_heading` — Heading Properties

```c
typedef struct {
  int internal_offset;      // Internal offset within the heading content
  int8_t level;             // Heading level (1-6)
  bool setext;              // Whether this is a setext-style heading (underlined)
} cmark_heading;
```

ATX headings (`# Heading`) have `setext = false`. Setext headings (underlined with `=` or `-`) have `setext = true`. The `level` field is shared and defaults to 1 when a heading node is created.

### `cmark_link` — Link and Image Properties

```c
typedef struct {
  unsigned char *url;       // Destination URL (separately allocated)
  unsigned char *title;     // Optional title text (separately allocated)
} cmark_link;
```

Both `url` and `title` are separately allocated strings that must be freed when the node is destroyed. This struct is used for both `CMARK_NODE_LINK` and `CMARK_NODE_IMAGE`.

### `cmark_custom` — Custom Block/Inline Properties

```c
typedef struct {
  unsigned char *on_enter;  // Literal text rendered when entering the node
  unsigned char *on_exit;   // Literal text rendered when leaving the node
} cmark_custom;
```

Custom nodes allow embedding arbitrary content in the AST for extensions. Both strings are separately allocated.

## Internal Flags

The `flags` field uses bit flags defined in the `cmark_node__internal_flags` enum:

```c
enum cmark_node__internal_flags {
  CMARK_NODE__OPEN              = (1 << 0),  // Block is still open (accepting content)
  CMARK_NODE__LAST_LINE_BLANK   = (1 << 1),  // Last line of this block was blank
  CMARK_NODE__LAST_LINE_CHECKED = (1 << 2),  // blank-line status has been computed
  CMARK_NODE__LIST_LAST_LINE_BLANK = (1 << 3), // (unused/reserved)
};
```

- **`CMARK_NODE__OPEN`**: Set when a block is created during parsing. Cleared by `finalize()` when the block is closed. The parser's `current` pointer always points to a node with this flag set.
- **`CMARK_NODE__LAST_LINE_BLANK`**: Set/cleared by `S_set_last_line_blank()` in `blocks.c` to track whether the most recent line added to this block was blank. Used for determining list tightness.
- **`CMARK_NODE__LAST_LINE_CHECKED`**: Prevents redundant traversal when checking `S_ends_with_blank_line()`, which recursively descends into list items.

## Node Creation

### `cmark_node_new_with_mem()`

The primary creation function (in `node.c`):

```c
cmark_node *cmark_node_new_with_mem(cmark_node_type type, cmark_mem *mem) {
  cmark_node *node = (cmark_node *)mem->calloc(1, sizeof(*node));
  node->mem = mem;
  node->type = (uint16_t)type;

  switch (node->type) {
  case CMARK_NODE_HEADING:
    node->as.heading.level = 1;
    break;
  case CMARK_NODE_LIST: {
    cmark_list *list = &node->as.list;
    list->list_type = CMARK_BULLET_LIST;
    list->start = 0;
    list->tight = false;
    break;
  }
  default:
    break;
  }

  return node;
}
```

The `calloc()` zeroes all fields, so pointers start as NULL and numeric fields as 0. Only heading and list nodes need explicit default initialization.

### `make_block()` — Parser-Internal Creation

During block parsing, `make_block()` in `blocks.c` creates nodes with source position and the `CMARK_NODE__OPEN` flag:

```c
static cmark_node *make_block(cmark_mem *mem, cmark_node_type tag,
                              int start_line, int start_column) {
  cmark_node *e;
  e = (cmark_node *)mem->calloc(1, sizeof(*e));
  e->mem = mem;
  e->type = (uint16_t)tag;
  e->flags = CMARK_NODE__OPEN;
  e->start_line = start_line;
  e->start_column = start_column;
  e->end_line = start_line;
  return e;
}
```

### Inline Node Creation

The inline parser in `inlines.c` uses two factory functions:

```c
// Create an inline with string content (text, code, HTML)
static inline cmark_node *make_literal(subject *subj, cmark_node_type t,
                                       int start_column, int end_column) {
  cmark_node *e = (cmark_node *)subj->mem->calloc(1, sizeof(*e));
  e->mem = subj->mem;
  e->type = (uint16_t)t;
  e->start_line = e->end_line = subj->line;
  e->start_column = start_column + 1 + subj->column_offset + subj->block_offset;
  e->end_column = end_column + 1 + subj->column_offset + subj->block_offset;
  return e;
}

// Create an inline with no value (emphasis, strong, etc.)
static inline cmark_node *make_simple(cmark_mem *mem, cmark_node_type t) {
  cmark_node *e = (cmark_node *)mem->calloc(1, sizeof(*e));
  e->mem = mem;
  e->type = t;
  return e;
}
```

## Node Destruction

### `S_free_nodes()` — Iterative Subtree Freeing

The `S_free_nodes()` function in `node.c` avoids recursion by splicing children into a flat linked list:

```c
static void S_free_nodes(cmark_node *e) {
  cmark_mem *mem = e->mem;
  cmark_node *next;
  while (e != NULL) {
    switch (e->type) {
    case CMARK_NODE_CODE_BLOCK:
      mem->free(e->data);
      mem->free(e->as.code.info);
      break;
    case CMARK_NODE_TEXT:
    case CMARK_NODE_HTML_INLINE:
    case CMARK_NODE_CODE:
    case CMARK_NODE_HTML_BLOCK:
      mem->free(e->data);
      break;
    case CMARK_NODE_LINK:
    case CMARK_NODE_IMAGE:
      mem->free(e->as.link.url);
      mem->free(e->as.link.title);
      break;
    case CMARK_NODE_CUSTOM_BLOCK:
    case CMARK_NODE_CUSTOM_INLINE:
      mem->free(e->as.custom.on_enter);
      mem->free(e->as.custom.on_exit);
      break;
    default:
      break;
    }
    if (e->last_child) {
      // Splice children into list for flat iteration
      e->last_child->next = e->next;
      e->next = e->first_child;
    }
    next = e->next;
    mem->free(e);
    e = next;
  }
}
```

This splicing technique converts the tree into a flat list, allowing O(n) iterative freeing without a recursion stack. For each node with children, the children are prepended to the remaining list by connecting `last_child->next` to `e->next` and `e->next` to `first_child`.

## Containership Rules

The `S_can_contain()` function in `node.c` enforces which node types can contain which children:

```c
static bool S_can_contain(cmark_node *node, cmark_node *child) {
  // Ancestor loop detection
  if (child->first_child != NULL) {
    cmark_node *cur = node->parent;
    while (cur != NULL) {
      if (cur == child) return false;
      cur = cur->parent;
    }
  }

  // Documents cannot be children
  if (child->type == CMARK_NODE_DOCUMENT) return false;

  switch (node->type) {
  case CMARK_NODE_DOCUMENT:
  case CMARK_NODE_BLOCK_QUOTE:
  case CMARK_NODE_ITEM:
    return cmark_node_is_block(child) && child->type != CMARK_NODE_ITEM;

  case CMARK_NODE_LIST:
    return child->type == CMARK_NODE_ITEM;

  case CMARK_NODE_CUSTOM_BLOCK:
    return true;  // Custom blocks can contain anything

  case CMARK_NODE_PARAGRAPH:
  case CMARK_NODE_HEADING:
  case CMARK_NODE_EMPH:
  case CMARK_NODE_STRONG:
  case CMARK_NODE_LINK:
  case CMARK_NODE_IMAGE:
  case CMARK_NODE_CUSTOM_INLINE:
    return cmark_node_is_inline(child);

  default:
    break;
  }
  return false;
}
```

Key rules:
- **Document, block quote, list item**: Can contain any block except items
- **List**: Can only contain items
- **Custom block**: Can contain anything (no restrictions)
- **Paragraph, heading, emphasis, strong, link, image, custom inline**: Can only contain inline nodes
- **Leaf blocks** (thematic break, code block, HTML block): Cannot contain anything

## Tree Manipulation

### Unlinking

The internal `S_node_unlink()` function detaches a node from its parent and siblings:

```c
static void S_node_unlink(cmark_node *node) {
  if (node->prev) {
    node->prev->next = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  }
  // Update parent's first_child / last_child pointers
  if (node->parent) {
    if (node->parent->first_child == node)
      node->parent->first_child = node->next;
    if (node->parent->last_child == node)
      node->parent->last_child = node->prev;
  }
  node->next = NULL;
  node->prev = NULL;
  node->parent = NULL;
}
```

### String Setting Helper

The `cmark_set_cstr()` function manages string assignment with proper memory handling:

```c
static bufsize_t cmark_set_cstr(cmark_mem *mem, unsigned char **dst,
                                const char *src) {
  unsigned char *old = *dst;
  bufsize_t len;
  if (src && src[0]) {
    len = (bufsize_t)strlen(src);
    *dst = (unsigned char *)mem->realloc(NULL, len + 1);
    memcpy(*dst, src, len + 1);
  } else {
    len = 0;
    *dst = NULL;
  }
  if (old) {
    mem->free(old);
  }
  return len;
}
```

This function allocates a new copy of the source string, assigns it, then frees the old value — ensuring no memory leaks even when overwriting existing data.

## Node Data Storage Pattern

Nodes store their text content in two ways depending on type:

1. **Direct storage** (`data` + `len`): Used by `CMARK_NODE_TEXT`, `CMARK_NODE_CODE`, `CMARK_NODE_CODE_BLOCK`, `CMARK_NODE_HTML_BLOCK`, and `CMARK_NODE_HTML_INLINE`. The `data` field points to a separately allocated buffer containing the text content.

2. **Union storage** (`as.*`): Used by lists, code blocks (for the info string), headings, links/images, and custom nodes. These store structured data rather than raw text.

3. **Hybrid**: `CMARK_NODE_CODE_BLOCK` uses both — `data` for the code content and `as.code.info` for the info string.

## The `cmark_node_check()` Function

For debug builds, `cmark_node_check()` validates the structural integrity of the tree. It checks that parent/child/sibling pointers are consistent and that the tree forms a valid structure. It returns the number of errors found and prints details to the provided `FILE*`.

## Cross-References

- [node.h](../../../cmark/src/node.h) — Struct definitions
- [node.c](../../../cmark/src/node.c) — Implementation
- [iterator-system.md](iterator-system.md) — How nodes are traversed
- [block-parsing.md](block-parsing.md) — How block nodes are created during parsing
- [inline-parsing.md](inline-parsing.md) — How inline nodes are created
- [memory-management.md](memory-management.md) — Allocator integration
