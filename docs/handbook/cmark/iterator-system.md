# cmark — Iterator System

## Overview

The iterator system (`iterator.c`, `iterator.h`) provides depth-first traversal of the AST using an event-based model. Each node is visited twice: once on `CMARK_EVENT_ENTER` (before children) and once on `CMARK_EVENT_EXIT` (after children). Leaf nodes receive both events in immediate succession.

All renderers (HTML, XML, LaTeX, man, CommonMark) use the iterator as their traversal mechanism.

## Public API

```c
cmark_iter *cmark_iter_new(cmark_node *root);
void cmark_iter_free(cmark_iter *iter);
cmark_event_type cmark_iter_next(cmark_iter *iter);
cmark_node *cmark_iter_get_node(cmark_iter *iter);
cmark_event_type cmark_iter_get_event_type(cmark_iter *iter);
cmark_node *cmark_iter_get_root(cmark_iter *iter);
void cmark_iter_reset(cmark_iter *iter, cmark_node *current, cmark_event_type event_type);
```

## Iterator State

```c
struct cmark_iter {
  cmark_mem *mem;
  cmark_node *root;
  cmark_node *cur;
  cmark_event_type ev_type;
};
```

The iterator stores:
- `root` — The subtree root (traversal boundary)
- `cur` — Current node
- `ev_type` — Current event (`CMARK_EVENT_ENTER`, `CMARK_EVENT_EXIT`, `CMARK_EVENT_DONE`, or `CMARK_EVENT_NONE`)

## Event Types

```c
typedef enum {
  CMARK_EVENT_NONE,     // Initial state
  CMARK_EVENT_DONE,     // Traversal complete (exited root)
  CMARK_EVENT_ENTER,    // Entering a node (pre-children)
  CMARK_EVENT_EXIT,     // Exiting a node (post-children)
} cmark_event_type;
```

## Leaf Node Detection

```c
static const int S_leaf_mask =
    (1 << CMARK_NODE_HTML_BLOCK) | (1 << CMARK_NODE_THEMATIC_BREAK) |
    (1 << CMARK_NODE_CODE_BLOCK) | (1 << CMARK_NODE_TEXT) |
    (1 << CMARK_NODE_SOFTBREAK)  | (1 << CMARK_NODE_LINEBREAK) |
    (1 << CMARK_NODE_CODE)       | (1 << CMARK_NODE_HTML_INLINE);

static bool S_is_leaf(cmark_node *node) {
  return ((1 << node->type) & S_leaf_mask) != 0;
}
```

Leaf nodes are determined by a bitmask — not by checking whether `first_child` is NULL. This means an emphasis node with no children is still treated as a container (it receives separate enter and exit events).

The leaf node types are:
- **Block leaves**: `HTML_BLOCK`, `THEMATIC_BREAK`, `CODE_BLOCK`
- **Inline leaves**: `TEXT`, `SOFTBREAK`, `LINEBREAK`, `CODE`, `HTML_INLINE`

## Traversal Algorithm

`cmark_iter_next()` implements the state machine:

```c
cmark_event_type cmark_iter_next(cmark_iter *iter) {
  cmark_event_type ev_type = iter->ev_type;
  cmark_node *cur = iter->cur;

  if (ev_type == CMARK_EVENT_DONE) {
    return CMARK_EVENT_DONE;
  }

  // For initial state, start with ENTER on root
  if (ev_type == CMARK_EVENT_NONE) {
    iter->ev_type = CMARK_EVENT_ENTER;
    return iter->ev_type;
  }

  if (ev_type == CMARK_EVENT_ENTER && !S_is_leaf(cur)) {
    // Container node being entered — descend to first child if it exists
    if (cur->first_child) {
      iter->ev_type = CMARK_EVENT_ENTER;
      iter->cur = cur->first_child;
    } else {
      // Empty container — immediately exit
      iter->ev_type = CMARK_EVENT_EXIT;
    }
  } else if (cur == iter->root) {
    // Exiting root (or leaf at root) — done
    iter->ev_type = CMARK_EVENT_DONE;
    iter->cur = NULL;
  } else if (cur->next) {
    // Move to next sibling
    iter->ev_type = CMARK_EVENT_ENTER;
    iter->cur = cur->next;
  } else if (cur->parent) {
    // No more siblings — exit parent
    iter->ev_type = CMARK_EVENT_EXIT;
    iter->cur = cur->parent;
  } else {
    // Orphan node — done
    assert(false);
    iter->ev_type = CMARK_EVENT_DONE;
    iter->cur = NULL;
  }

  return iter->ev_type;
}
```

### State Transition Summary

| Current State | Condition | Next State |
|--------------|-----------|------------|
| `NONE` | (initial) | `ENTER(root)` |
| `ENTER(container)` | has children | `ENTER(first_child)` |
| `ENTER(container)` | no children | `EXIT(container)` |
| `ENTER(leaf)` or `EXIT(node)` | node == root | `DONE` |
| `ENTER(leaf)` or `EXIT(node)` | has next sibling | `ENTER(next)` |
| `ENTER(leaf)` or `EXIT(node)` | has parent | `EXIT(parent)` |
| `DONE` | (terminal) | `DONE` |

### Traversal Order Example

For a document with a paragraph containing "Hello *world*":

```
Document
└── Paragraph
    ├── Text("Hello ")
    ├── Emph
    │   └── Text("world")
    └── (end)
```

Event sequence:
1. `ENTER(Document)`
2. `ENTER(Paragraph)`
3. `ENTER(Text "Hello ")` — leaf, immediate transition
4. `ENTER(Emph)`
5. `ENTER(Text "world")` — leaf, immediate transition
6. `EXIT(Emph)`
7. `EXIT(Paragraph)`
8. `EXIT(Document)`
9. `DONE`

## Iterator Reset

```c
void cmark_iter_reset(cmark_iter *iter, cmark_node *current,
                      cmark_event_type event_type) {
  iter->cur = current;
  iter->ev_type = event_type;
}
```

Allows repositioning the iterator to any node and event type. This is used by renderers to skip subtrees — e.g., when the HTML renderer processes an image node, it may skip children after extracting alt text.

## Text Node Consolidation

```c
void cmark_consolidate_text_nodes(cmark_node *root) {
  if (root == NULL) return;
  cmark_iter *iter = cmark_iter_new(root);
  cmark_strbuf buf = CMARK_BUF_INIT(iter->mem);
  cmark_event_type ev_type;
  cmark_node *cur, *tmp, *next;

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    if (ev_type == CMARK_EVENT_ENTER && cur->type == CMARK_NODE_TEXT &&
        cur->next && cur->next->type == CMARK_NODE_TEXT) {
      // Merge consecutive text nodes
      cmark_strbuf_clear(&buf);
      cmark_strbuf_put(&buf, cur->data, cur->len);
      tmp = cur->next;
      while (tmp && tmp->type == CMARK_NODE_TEXT) {
        cmark_iter_reset(iter, tmp, CMARK_EVENT_ENTER);
        cmark_strbuf_put(&buf, tmp->data, tmp->len);
        cur->end_column = tmp->end_column;
        next = tmp->next;
        cmark_node_free(tmp);
        tmp = next;
      }
      // Replace cur's data with merged content
      cmark_chunk_free(iter->mem, &cur->as.literal);
      cmark_strbuf_trim(&buf);
      // ... set cur->data and cur->len
    }
  }
  cmark_strbuf_free(&buf);
  cmark_iter_free(iter);
}
```

This function merges adjacent text nodes into a single text node. Adjacent text nodes can arise from inline parsing (e.g., when backslash escapes split text). The function:

1. Finds consecutive text node runs
2. Concatenates their content into a buffer
3. Updates the first node's content and end position
4. Frees the subsequent nodes
5. Uses `cmark_iter_reset()` to skip freed nodes

## Usage Patterns

### Standard Rendering Loop

```c
cmark_iter *iter = cmark_iter_new(root);
while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
  cur = cmark_iter_get_node(iter);
  S_render_node(cur, ev_type, &state, options);
}
cmark_iter_free(iter);
```

### Skipping Children

To skip rendering of a node's children (e.g., for image alt text in HTML):
```c
if (ev_type == CMARK_EVENT_ENTER) {
  cmark_iter_reset(iter, node, CMARK_EVENT_EXIT);
}
```

This jumps directly to the exit event, bypassing all children.

### Safe Node Removal

The iterator handles node removal between calls. Since `cmark_iter_next()` always follows `next` and `parent` pointers from the current position, removing the current node is safe as long as:
1. The node's `next` and `parent` pointers remain valid
2. The iterator is reset to skip the removed node's children

## Thread Safety

Iterators are NOT thread-safe. A single AST must not be iterated concurrently without external synchronization. However, since iterators only read the AST (never modify it), multiple read-only iterators on the same AST are safe if no modifications occur.

## Memory

The iterator allocates a `cmark_iter` struct using the root node's memory allocator:
```c
cmark_iter *cmark_iter_new(cmark_node *root) {
  cmark_mem *mem = root->mem;
  cmark_iter *iter = (cmark_iter *)mem->calloc(1, sizeof(cmark_iter));
  iter->mem = mem;
  iter->root = root;
  iter->cur = root;
  iter->ev_type = CMARK_EVENT_NONE;
  return iter;
}
```

## Cross-References

- [iterator.c](../../cmark/src/iterator.c) — Iterator implementation
- [iterator.h](../../cmark/src/iterator.h) — Iterator struct definition
- [ast-node-system.md](ast-node-system.md) — The nodes being traversed
- [html-renderer.md](html-renderer.md) — Example of iterator-driven rendering
- [render-framework.md](render-framework.md) — Framework that wraps iterator use
