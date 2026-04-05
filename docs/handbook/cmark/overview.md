# cmark — Overview

## What Is cmark?

cmark is a C library and command-line tool for parsing and rendering CommonMark (standardized Markdown). Written in C99, it implements a two-phase parsing architecture — block structure recognition followed by inline content parsing — producing an Abstract Syntax Tree (AST) that can be traversed, manipulated, and rendered into multiple output formats.

**Language:** C (C99)  
**Build System:** CMake (minimum version 3.14)  
**Project Version:** 0.31.2  
**License:** BSD-2-Clause  
**Authors:** John MacFarlane, Vicent Marti, Kārlis Gaņģis, Nick Wellnhofer

## Core Architecture Summary

cmark's processing pipeline follows this sequence:

1. **Input** — UTF-8 text is fed to the parser, either all at once or incrementally via a streaming API.
2. **Block Parsing** (`blocks.c`) — The input is scanned line-by-line to identify block-level structures (paragraphs, headings, code blocks, lists, block quotes, thematic breaks, HTML blocks).
3. **Inline Parsing** (`inlines.c`) — Within paragraph and heading blocks, inline elements are parsed (emphasis, links, images, code spans, HTML inline, line breaks).
4. **AST Construction** — A tree of `cmark_node` structures is built, with each node representing a document element.
5. **Rendering** — The AST is traversed using an iterator and rendered to one of five output formats: HTML, XML, LaTeX, man (groff), or CommonMark.

## Source File Map

The `cmark/src/` directory contains the following source files, organized by responsibility:

### Public API
| File | Purpose |
|------|---------|
| `cmark.h` | Public API header — all exported types, enums, and function declarations |
| `cmark.c` | Core glue — `cmark_markdown_to_html()`, default memory allocator, version info |
| `main.c` | CLI entry point — argument parsing, file I/O, format dispatch |

### AST Node System
| File | Purpose |
|------|---------|
| `node.h` | Internal node struct definition, type-specific unions (`cmark_list`, `cmark_code`, `cmark_heading`, `cmark_link`, `cmark_custom`), internal flags |
| `node.c` | Node creation/destruction, accessor functions, tree manipulation (insert, append, unlink, replace) |

### Parsing
| File | Purpose |
|------|---------|
| `parser.h` | Internal `cmark_parser` struct definition (parser state: line number, offset, column, indent, reference map) |
| `blocks.c` | Block-level parsing — line-by-line analysis, open/close block logic, list item detection, finalization |
| `inlines.c` | Inline-level parsing — emphasis/strong via delimiter stack, backtick code spans, links/images via bracket stack, autolinks, HTML inline |
| `inlines.h` | Internal API: `cmark_parse_inlines()`, `cmark_parse_reference_inline()`, `cmark_clean_url()`, `cmark_clean_title()` |

### Traversal
| File | Purpose |
|------|---------|
| `iterator.h` | Internal `cmark_iter` struct with `cmark_iter_state` (current + next event/node pairs) |
| `iterator.c` | Iterator implementation — `cmark_iter_new()`, `cmark_iter_next()`, `cmark_iter_reset()`, `cmark_consolidate_text_nodes()` |

### Renderers
| File | Purpose |
|------|---------|
| `render.h` | `cmark_renderer` struct, `cmark_escaping` enum (`LITERAL`, `NORMAL`, `TITLE`, `URL`) |
| `render.c` | Generic render framework — line wrapping, prefix management, `cmark_render()` dispatch loop |
| `html.c` | HTML renderer — `cmark_render_html()`, direct strbuf-based output, no render framework |
| `xml.c` | XML renderer — `cmark_render_xml()`, direct strbuf-based output with CommonMark DTD |
| `latex.c` | LaTeX renderer — `cmark_render_latex()`, uses render framework |
| `man.c` | groff man renderer — `cmark_render_man()`, uses render framework |
| `commonmark.c` | CommonMark renderer — `cmark_render_commonmark()`, uses render framework |

### Text Processing and Utilities
| File | Purpose |
|------|---------|
| `buffer.h` / `buffer.c` | `cmark_strbuf` — growable byte buffer with amortized O(1) append |
| `chunk.h` | `cmark_chunk` — lightweight non-owning string slice (pointer + length) |
| `utf8.h` / `utf8.c` | UTF-8 iteration, validation, encoding, case folding, Unicode property queries |
| `references.h` / `references.c` | Link reference definition storage and lookup (sorted array with binary search) |
| `scanners.h` / `scanners.c` | re2c-generated scanner functions for recognizing Markdown syntax patterns |
| `scanners.re` | re2c source for scanner generation |
| `cmark_ctype.h` / `cmark_ctype.c` | Locale-independent `cmark_isspace()`, `cmark_ispunct()`, `cmark_isdigit()`, `cmark_isalpha()` |
| `houdini.h` | HTML/URL escaping and unescaping API |
| `houdini_html_e.c` | HTML entity escaping |
| `houdini_html_u.c` | HTML entity unescaping |
| `houdini_href_e.c` | URL/href percent-encoding |
| `entities.inc` | HTML entity name-to-codepoint lookup table |
| `case_fold.inc` | Unicode case folding table for reference normalization |

## The Simple Interface

The simplest way to use cmark is a single function call defined in `cmark.c`:

```c
char *cmark_markdown_to_html(const char *text, size_t len, int options);
```

Internally, this calls `cmark_parse_document()` to build the AST, then `cmark_render_html()` to produce the output, and finally frees the document node. The caller is responsible for freeing the returned string.

The implementation in `cmark.c`:

```c
char *cmark_markdown_to_html(const char *text, size_t len, int options) {
  cmark_node *doc;
  char *result;

  doc = cmark_parse_document(text, len, options);
  result = cmark_render_html(doc, options);
  cmark_node_free(doc);

  return result;
}
```

## The Streaming Interface

For large documents or streaming input, cmark provides an incremental parsing API:

```c
cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);

// Feed chunks of data as they arrive
while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
    cmark_parser_feed(parser, buffer, bytes);
}

// Finalize and get the AST
cmark_node *document = cmark_parser_finish(parser);
cmark_parser_free(parser);

// Render to any format
char *html = cmark_render_html(document, CMARK_OPT_DEFAULT);
char *xml  = cmark_render_xml(document, CMARK_OPT_DEFAULT);
char *man  = cmark_render_man(document, CMARK_OPT_DEFAULT, 72);
char *tex  = cmark_render_latex(document, CMARK_OPT_DEFAULT, 80);
char *cm   = cmark_render_commonmark(document, CMARK_OPT_DEFAULT, 0);

// Cleanup
cmark_node_free(document);
```

The parser accumulates input in an internal line buffer (`parser->linebuf`) and processes complete lines as they become available. The `S_parser_feed()` function in `blocks.c` scans for line-ending characters (`\n`, `\r`) and dispatches each complete line to `S_process_line()`.

## Node Type Taxonomy

cmark defines 21 node types in the `cmark_node_type` enum:

### Block Nodes (container and leaf)
| Enum Value | Type String | Container? | Accepts Lines? | Contains Inlines? |
|-----------|-------------|------------|---------------|-------------------|
| `CMARK_NODE_DOCUMENT` | "document" | Yes | No | No |
| `CMARK_NODE_BLOCK_QUOTE` | "block_quote" | Yes | No | No |
| `CMARK_NODE_LIST` | "list" | Yes (items only) | No | No |
| `CMARK_NODE_ITEM` | "item" | Yes | No | No |
| `CMARK_NODE_CODE_BLOCK` | "code_block" | No (leaf) | Yes | No |
| `CMARK_NODE_HTML_BLOCK` | "html_block" | No (leaf) | No | No |
| `CMARK_NODE_CUSTOM_BLOCK` | "custom_block" | Yes | No | No |
| `CMARK_NODE_PARAGRAPH` | "paragraph" | No | Yes | Yes |
| `CMARK_NODE_HEADING` | "heading" | No | Yes | Yes |
| `CMARK_NODE_THEMATIC_BREAK` | "thematic_break" | No (leaf) | No | No |

### Inline Nodes
| Enum Value | Type String | Leaf? |
|-----------|-------------|-------|
| `CMARK_NODE_TEXT` | "text" | Yes |
| `CMARK_NODE_SOFTBREAK` | "softbreak" | Yes |
| `CMARK_NODE_LINEBREAK` | "linebreak" | Yes |
| `CMARK_NODE_CODE` | "code" | Yes |
| `CMARK_NODE_HTML_INLINE` | "html_inline" | Yes |
| `CMARK_NODE_CUSTOM_INLINE` | "custom_inline" | No |
| `CMARK_NODE_EMPH` | "emph" | No |
| `CMARK_NODE_STRONG` | "strong" | No |
| `CMARK_NODE_LINK` | "link" | No |
| `CMARK_NODE_IMAGE` | "image" | No |

Range sentinels are also defined for classification:
- `CMARK_NODE_FIRST_BLOCK = CMARK_NODE_DOCUMENT`
- `CMARK_NODE_LAST_BLOCK = CMARK_NODE_THEMATIC_BREAK`
- `CMARK_NODE_FIRST_INLINE = CMARK_NODE_TEXT`
- `CMARK_NODE_LAST_INLINE = CMARK_NODE_IMAGE`

## Option Flags

Options are passed as a bitmask integer to parsing and rendering functions:

```c
#define CMARK_OPT_DEFAULT       0
#define CMARK_OPT_SOURCEPOS     (1 << 1)   // Add data-sourcepos attributes
#define CMARK_OPT_HARDBREAKS    (1 << 2)   // Render softbreaks as hard breaks
#define CMARK_OPT_SAFE          (1 << 3)   // Legacy (now default behavior)
#define CMARK_OPT_NOBREAKS      (1 << 4)   // Render softbreaks as spaces
#define CMARK_OPT_NORMALIZE     (1 << 8)   // Legacy (no effect)
#define CMARK_OPT_VALIDATE_UTF8 (1 << 9)   // Validate UTF-8 input
#define CMARK_OPT_SMART         (1 << 10)  // Smart quotes and dashes
#define CMARK_OPT_UNSAFE        (1 << 17)  // Allow raw HTML and dangerous URLs
```

## Memory Management Model

cmark uses a pluggable memory allocator defined by the `cmark_mem` struct:

```c
typedef struct cmark_mem {
  void *(*calloc)(size_t, size_t);
  void *(*realloc)(void *, size_t);
  void (*free)(void *);
} cmark_mem;
```

The default allocator in `cmark.c` wraps standard `calloc`/`realloc`/`free` with abort-on-NULL safety checks (`xcalloc`, `xrealloc`). Every node stores a pointer to the allocator it was created with (`node->mem`), ensuring consistent allocation/deallocation throughout the tree.

## Version Information

Runtime version queries:

```c
int cmark_version(void);               // Returns CMARK_VERSION as integer (0xMMmmpp)
const char *cmark_version_string(void); // Returns CMARK_VERSION_STRING
```

The version is encoded as a 24-bit integer where bits 16–23 are major, 8–15 are minor, and 0–7 are patch. For example, `0x001F02` represents version 0.31.2.

## Backwards Compatibility Aliases

For code written against older cmark API versions, these aliases are provided:

```c
#define CMARK_NODE_HEADER      CMARK_NODE_HEADING
#define CMARK_NODE_HRULE       CMARK_NODE_THEMATIC_BREAK
#define CMARK_NODE_HTML        CMARK_NODE_HTML_BLOCK
#define CMARK_NODE_INLINE_HTML CMARK_NODE_HTML_INLINE
```

Short-name aliases (without the `CMARK_` prefix) are also available unless `CMARK_NO_SHORT_NAMES` is defined:

```c
#define NODE_DOCUMENT    CMARK_NODE_DOCUMENT
#define NODE_PARAGRAPH   CMARK_NODE_PARAGRAPH
#define BULLET_LIST      CMARK_BULLET_LIST
// ... and many more
```

## Cross-References

- [architecture.md](architecture.md) — Detailed two-phase parsing pipeline, module dependency graph
- [public-api.md](public-api.md) — Complete public API reference with all function signatures
- [ast-node-system.md](ast-node-system.md) — Internal `cmark_node` struct, type-specific unions, tree operations
- [block-parsing.md](block-parsing.md) — `blocks.c` line-by-line analysis, open block tracking, finalization
- [inline-parsing.md](inline-parsing.md) — `inlines.c` delimiter algorithm, bracket stack, backtick scanning
- [iterator-system.md](iterator-system.md) — AST traversal with enter/exit events
- [html-renderer.md](html-renderer.md) — HTML output with escaping and source position
- [xml-renderer.md](xml-renderer.md) — XML output with CommonMark DTD
- [latex-renderer.md](latex-renderer.md) — LaTeX output via render framework
- [man-renderer.md](man-renderer.md) — groff man page output
- [commonmark-renderer.md](commonmark-renderer.md) — Round-trip CommonMark output
- [render-framework.md](render-framework.md) — Shared `cmark_render()` engine for text-based renderers
- [memory-management.md](memory-management.md) — Allocator model, buffer growth, node freeing
- [utf8-handling.md](utf8-handling.md) — UTF-8 validation, iteration, case folding
- [reference-system.md](reference-system.md) — Link reference definitions storage and resolution
- [scanner-system.md](scanner-system.md) — re2c-generated pattern matching
- [building.md](building.md) — CMake build configuration and options
- [cli-usage.md](cli-usage.md) — Command-line tool usage
- [testing.md](testing.md) — Test infrastructure (spec tests, API tests, fuzzing)
- [code-style.md](code-style.md) — Coding conventions and naming patterns
