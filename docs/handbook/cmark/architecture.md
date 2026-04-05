# cmark — Architecture

## High-Level Design

cmark implements a two-phase parsing pipeline that converts CommonMark Markdown into an Abstract Syntax Tree (AST), which can then be rendered into multiple output formats. The design separates concerns cleanly: block-level structure is identified first, then inline content is parsed within the appropriate blocks.

```
Input Text (UTF-8)
    │
    ▼
┌──────────────────┐
│   S_parser_feed   │  Split input into lines (blocks.c)
│                    │  Handle UTF-8 BOM, CR/LF normalization
└────────┬───────────┘
         │
         ▼
┌──────────────────┐
│  S_process_line   │  Line-by-line block structure analysis (blocks.c)
│                    │  Open/close containers, detect leaf blocks
└────────┬───────────┘
         │
         ▼
┌──────────────────┐
│  finalize_document│  Close all open blocks (blocks.c)
│                    │  Resolve reference link definitions
└────────┬───────────┘
         │
         ▼
┌──────────────────┐
│  process_inlines  │  Parse inline content in paragraphs/headings (blocks.c → inlines.c)
│                    │  Delimiter stack algorithm for emphasis
│                    │  Bracket stack for links/images
└────────┬───────────┘
         │
         ▼
┌──────────────────┐
│    AST (cmark_node tree)    │
└────────┬───────────┘
         │
         ▼
┌──────────────────┐
│    Renderer       │  Iterator-driven traversal
│   (html/xml/      │  Enter/Exit events per node
│    latex/man/cm)   │
└──────────────────┘
         │
         ▼
    Output String
```

## Module Dependency Graph

The internal header dependencies reveal the layered architecture:

```
cmark.h (public API — types, enums, function declarations)
  ├── cmark_export.h (generated — DLL export macros)
  └── cmark_version.h (generated — version constants)

node.h (internal — struct cmark_node)
  ├── cmark.h
  └── buffer.h

parser.h (internal — struct cmark_parser)
  ├── references.h
  ├── node.h
  └── buffer.h

iterator.h (internal — struct cmark_iter)
  └── cmark.h

render.h (internal — struct cmark_renderer)
  └── buffer.h

buffer.h (internal — cmark_strbuf)
  └── cmark.h

chunk.h (internal — cmark_chunk)
  ├── cmark.h
  ├── buffer.h
  └── cmark_ctype.h

references.h (internal — cmark_reference_map)
  └── chunk.h

inlines.h (internal — inline parsing API)
  ├── chunk.h
  └── references.h

scanners.h (internal — scanner function declarations)
  ├── cmark.h
  └── chunk.h

houdini.h (internal — HTML/URL escaping)
  └── buffer.h

cmark_ctype.h (internal — locale-independent char classification)
  (no cmark dependencies)

utf8.h (internal — UTF-8 processing)
  └── buffer.h
```

## Phase 1: Block Structure (blocks.c)

Block parsing operates on a state machine maintained in the `cmark_parser` struct (defined in `parser.h`):

```c
struct cmark_parser {
  struct cmark_mem *mem;                // Memory allocator
  struct cmark_reference_map *refmap;   // Link reference definitions
  struct cmark_node *root;              // Document root node
  struct cmark_node *current;           // Deepest open block
  int line_number;                      // Current line being processed
  bufsize_t offset;                     // Byte position in current line
  bufsize_t column;                     // Virtual column (tabs expanded)
  bufsize_t first_nonspace;             // Position of first non-whitespace
  bufsize_t first_nonspace_column;      // Column of first non-whitespace
  bufsize_t thematic_break_kill_pos;    // Optimization for thematic break scanning
  int indent;                           // Indentation level (first_nonspace_column - column)
  bool blank;                           // Whether current line is blank
  bool partially_consumed_tab;          // Tab only partially used for indentation
  cmark_strbuf curline;                 // Current line being processed
  bufsize_t last_line_length;           // Length of previous line (for end_column)
  cmark_strbuf linebuf;                 // Buffer for accumulating partial lines across feeds
  cmark_strbuf content;                 // Accumulated content for the current open block
  int options;                          // Option flags
  bool last_buffer_ended_with_cr;       // For CR/LF handling across buffer boundaries
  unsigned int total_size;              // Total bytes fed (for reference expansion limiting)
};
```

### Line Processing Flow

For each line, `S_process_line()` does the following:

1. **Increment line number**, store current line in `parser->curline`.
2. **Check open blocks** (`check_open_blocks()`): Walk through the tree from root to the deepest open node. For each open container node, try to match the expected line prefix:
   - Block quote: expect `>` (optionally preceded by up to 3 spaces)
   - List item: expect indentation matching `marker_offset + padding`
   - Code block (fenced): check for closing fence or skip fence offset spaces
   - Code block (indented): expect 4+ spaces of indentation
   - HTML block: check type-specific continuation rules
3. **Try new container starts**: If not all open blocks matched, check if the current line starts a new container (block quote, list item).
4. **Try new leaf blocks**: If the line doesn't continue an existing block or start a new container, check for:
   - ATX heading (lines starting with 1-6 `#` characters)
   - Setext heading (underlines of `=` or `-` following a paragraph)
   - Thematic break (3+ `*`, `-`, or `_` on a line by themselves)
   - Fenced code block (3+ backticks or tildes)
   - HTML block (7 different start patterns)
   - Indented code block (4+ spaces of indentation)
5. **Add line content**: For blocks that accept lines (paragraph, heading, code block), append the line content to `parser->content`.
6. **Handle lazy continuation**: Paragraphs support lazy continuation where a non-blank line can continue a paragraph even without matching container prefixes.

### Finalization

When a block is closed (either explicitly or because a new block replaces it), `finalize()` is called:

- **Paragraphs**: Reference link definitions at the start are extracted and stored in `parser->refmap`. If only references remain, the paragraph node is deleted.
- **Code blocks (fenced)**: The first line becomes the info string; remaining content becomes the code body.
- **Code blocks (indented)**: Trailing blank lines are removed.
- **Lists**: Tight/loose status is determined by checking for blank lines between items and their children.

## Phase 2: Inline Parsing (inlines.c)

After all block structure is finalized, `process_inlines()` walks the AST with an iterator and calls `cmark_parse_inlines()` for every node whose type `contains_inlines()` — specifically, `CMARK_NODE_PARAGRAPH` and `CMARK_NODE_HEADING`.

The inline parser uses a `subject` struct that tracks:

```c
typedef struct {
  cmark_mem *mem;
  cmark_chunk input;              // The text to parse
  unsigned flags;                 // Skip flags for HTML constructs
  int line;                       // Source line number
  bufsize_t pos;                  // Current position in input
  int block_offset;               // Column offset of containing block
  int column_offset;              // Adjustment for multi-line inlines
  cmark_reference_map *refmap;    // Reference definitions
  delimiter *last_delim;          // Top of delimiter stack
  bracket *last_bracket;          // Top of bracket stack
  bufsize_t backticks[MAXBACKTICKS + 1];  // Cache of backtick positions
  bool scanned_for_backticks;     // Whether full backtick scan done
  bool no_link_openers;           // Optimization flag
} subject;
```

### Delimiter Stack Algorithm

Emphasis (`*`, `_`) and smart quotes (`'`, `"`) use a delimiter stack. When a run of delimiter characters is found:

1. `scan_delims()` determines whether the run can open and/or close emphasis, based on Unicode-aware flanking rules (checking whether surrounding characters are spaces or punctuation using `cmark_utf8proc_is_space()` and `cmark_utf8proc_is_punctuation_or_symbol()`).
2. The delimiter is pushed onto the stack as a `delimiter` struct.
3. When a closing delimiter is found, the stack is scanned backwards for a matching opener, and `S_insert_emph()` creates `CMARK_NODE_EMPH` or `CMARK_NODE_STRONG` nodes.

### Bracket Stack Algorithm

Links and images use a separate bracket stack:

1. `[` pushes a bracket entry; `![` pushes one marked as `image = true`.
2. When `]` is encountered, the bracket stack is searched for a matching opener.
3. If found, the parser looks for `(url "title")` or `[ref]` after the `]`.
4. For reference-style links, `cmark_reference_lookup()` is called against the parser's `refmap`.

## Phase 3: AST Rendering

All renderers traverse the AST using the iterator system. There are two rendering architectures:

### Direct Renderers (no framework)
- **HTML** (`html.c`): Uses `cmark_strbuf` directly. The `S_render_node()` function handles enter/exit events in a large switch statement. HTML escaping is done via `houdini_escape_html()`.
- **XML** (`xml.c`): Similar direct approach with XML-specific escaping and indentation tracking.

### Framework Renderers (via render.c)
- **LaTeX** (`latex.c`), **man** (`man.c`), **CommonMark** (`commonmark.c`): These use the `cmark_render()` generic framework, which provides:
  - Line wrapping at a configurable width
  - Prefix management for indented output (block quotes, list items)
  - Breakpoint tracking for intelligent line breaking
  - Escape dispatch via function pointers (`outc`)

The framework signature:

```c
char *cmark_render(cmark_node *root, int options, int width,
                   void (*outc)(cmark_renderer *, cmark_escaping, int32_t, unsigned char),
                   int (*render_node)(cmark_renderer *, cmark_node *,
                                      cmark_event_type, int));
```

Each format-specific renderer supplies its own `outc` (character-level escaping) and `render_node` (node-level output) callback functions.

## Key Design Decisions

### Owning vs. Non-Owning Strings

cmark uses two string types:

- **`cmark_strbuf`** (buffer.h): Owning, growable byte buffer. Used for accumulating output and parser state. Memory is managed via the `cmark_mem` allocator.
- **`cmark_chunk`** (chunk.h): Non-owning slice (pointer + length). Used for referencing substrings of the input during parsing without copying.

### Node Memory Layout

Every `cmark_node` uses a discriminated union (`node->as`) to store type-specific data without separate allocations:

```c
union {
  cmark_list list;       // list marker, start, tight, delimiter
  cmark_code code;       // info string, fence char/length/offset
  cmark_heading heading; // level, setext flag, internal_offset
  cmark_link link;       // url, title
  cmark_custom custom;   // on_enter, on_exit
  int html_block_type;   // HTML block type (1-7)
} as;
```

### Open Block Tracking

During block parsing, open blocks are tracked via the `CMARK_NODE__OPEN` flag in `node->flags`. The parser maintains a `current` pointer to the deepest open block. When new blocks are created, they're added as children of the appropriate open container. When blocks are finalized (closed), control returns to the parent.

### Reference Expansion Limiting

To prevent superlinear growth from adversarial reference definitions, `parser->total_size` tracks total bytes fed. After finalization, `parser->refmap->max_ref_size` is set to `MAX(total_size, 100000)`, and each reference lookup deducts the reference's size from the available budget.

## Error Handling

cmark follows a defensive programming model:
- NULL checks on all public API entry points (return 0 or NULL for invalid arguments)
- `assert()` for internal invariants (only active in debug builds with `-DCMARK_DEBUG_NODES`)
- Abort-on-allocation-failure in the default memory allocator
- No exceptions (pure C99)
- Invalid UTF-8 sequences are replaced with U+FFFD (when `CMARK_OPT_VALIDATE_UTF8` is set)

## Thread Safety

cmark is **not** thread-safe for concurrent access to the same parser or node tree. However, separate parser instances and separate node trees can be used in parallel from different threads, as there is no global mutable state (the `DEFAULT_MEM_ALLOCATOR` is read-only after initialization).

## Cross-References

- [block-parsing.md](block-parsing.md) — Detailed block-level parsing logic
- [inline-parsing.md](inline-parsing.md) — Delimiter and bracket stack algorithms
- [ast-node-system.md](ast-node-system.md) — Node struct internals
- [render-framework.md](render-framework.md) — Generic render engine
- [memory-management.md](memory-management.md) — Allocator and buffer details
- [iterator-system.md](iterator-system.md) — AST traversal mechanics
