# cmark — Block Parsing

## Overview

Block parsing is Phase 1 of cmark's two-phase parsing pipeline. Implemented in `blocks.c`, it processes the input line-by-line, identifying block-level document structure: paragraphs, headings, code blocks, block quotes, lists, thematic breaks, and HTML blocks. The result is a tree of `cmark_node` block nodes with accumulated text content. Inline parsing occurs in Phase 2.

The algorithm follows the CommonMark specification's description at `http://spec.commonmark.org/0.24/#phase-1-block-structure`.

## Key Constants

```c
#define CODE_INDENT 4   // Spaces required for indented code block
#define TAB_STOP 4      // Tab stop width for column calculation
```

## Parser State

The parser state is maintained in the `cmark_parser` struct (from `parser.h`). During line processing, these fields track the current position:

- `offset` — byte position in the current line
- `column` — virtual column number (tabs expanded to `TAB_STOP` boundaries)
- `first_nonspace` — byte position of first non-whitespace character
- `first_nonspace_column` — column of first non-whitespace character
- `indent` — the difference `first_nonspace_column - column`, representing effective indentation
- `blank` — whether the line is blank (only whitespace before line end)
- `partially_consumed_tab` — set when a tab is only partially used for indentation

## Input Feeding: `S_parser_feed()`

The entry point for input is `S_parser_feed()`, which splits raw input into lines:

```c
static void S_parser_feed(cmark_parser *parser, const unsigned char *buffer,
                          size_t len, bool eof);
```

### Line Splitting Logic

The function scans for line-ending characters (`\n`, `\r`) and processes complete lines via `S_process_line()`. Partial lines are accumulated in `parser->linebuf`.

Key handling:
1. **UTF-8 BOM**: Skipped if found at the start of the first line (3-byte sequence `0xEF 0xBB 0xBF`).
2. **CR/LF across buffer boundaries**: If the previous buffer ended with `\r` and the next starts with `\n`, the `\n` is skipped.
3. **NULL bytes**: Replaced with the UTF-8 replacement character (U+FFFD, `0xEF 0xBF 0xBD`).
4. **Total size tracking**: `parser->total_size` accumulates bytes fed, capped at `UINT_MAX`, used later for reference expansion limiting.

### Line Termination

Each line is terminated at `\n`, `\r`, or `\r\n`. The line content passed to `S_process_line()` does NOT include the line-ending characters themselves.

## Line Processing: `S_process_line()`

The main per-line processing function. For each line, it:

1. Stores the line in `parser->curline`
2. Creates a `cmark_chunk` wrapper for the line data
3. Increments `parser->line_number`
4. Calls `check_open_blocks()` to match existing containers
5. Attempts to open new containers and leaf blocks
6. Adds line content to the appropriate block

### Step 1: Check Open Blocks

```c
static cmark_node *check_open_blocks(cmark_parser *parser, cmark_chunk *input,
                                     bool *all_matched);
```

Starting from the document root, this walks through the tree of open blocks (following `last_child` pointers). For each open container, it tries to match the expected line prefix.

The matching rules for each container type:

#### Block Quote
```c
static bool parse_block_quote_prefix(cmark_parser *parser, cmark_chunk *input);
```
Expects `>` preceded by up to 3 spaces of indentation. After matching the `>`, optionally consumes one space or tab after it.

#### List Item
```c
static bool parse_node_item_prefix(cmark_parser *parser, cmark_chunk *input,
                                   cmark_node *container);
```
Expects indentation of at least `marker_offset + padding` characters. If the line is blank and the item has at least one child, the item continues (lazy continuation).

#### Fenced Code Block
```c
static bool parse_code_block_prefix(cmark_parser *parser, cmark_chunk *input,
                                    cmark_node *container, bool *should_continue);
```
For fenced code blocks: checks if the line is a closing fence (same fence char, length >= opening fence length, preceded by up to 3 spaces). If it is, the block is finalized. Otherwise, skips up to `fence_offset` spaces and continues.

For indented code blocks: requires 4+ spaces of indentation, or a blank line.

#### HTML Block
```c
static bool parse_html_block_prefix(cmark_parser *parser, cmark_node *container);
```
HTML block types 1-5 accept blank lines (continue until end condition is met). Types 6-7 do NOT accept blank lines.

### Step 2: New Container Starts

If not all open blocks were matched (`!all_matched`), the parser checks if the unmatched portion of the line starts a new container:

- **Block quote**: Line starts with `>` (preceded by up to 3 spaces)
- **List item**: Line starts with a list marker (bullet character or ordered number + delimiter)

### Step 3: New Leaf Blocks

The parser checks for new leaf block starts using scanner functions:

- **ATX heading**: `scan_atx_heading_start()` — lines starting with 1-6 `#` characters
- **Fenced code block**: `scan_open_code_fence()` — 3+ backticks or tildes
- **HTML block**: `scan_html_block_start()` and `scan_html_block_start_7()` — 7 different HTML start patterns
- **Setext heading**: `scan_setext_heading_line()` — underlines of `=` or `-` (only when following a paragraph)
- **Thematic break**: `S_scan_thematic_break()` — 3+ `*`, `-`, or `_` characters

### Step 4: Content Accumulation

For blocks that accept lines (`accepts_lines()` returns true for paragraphs, headings, and code blocks), the line content is appended to `parser->content` via `add_line()`:

```c
static void add_line(cmark_chunk *ch, cmark_parser *parser) {
  int chars_to_tab;
  int i;
  if (parser->partially_consumed_tab) {
    parser->offset += 1; // skip over tab
    chars_to_tab = TAB_STOP - (parser->column % TAB_STOP);
    for (i = 0; i < chars_to_tab; i++) {
      cmark_strbuf_putc(&parser->content, ' ');
    }
  }
  cmark_strbuf_put(&parser->content, ch->data + parser->offset,
                   ch->len - parser->offset);
}
```

When a tab is only partially consumed (e.g., the tab represents 4 columns but only 1 was needed for indentation), the remaining columns are emitted as spaces.

## Adding Child Blocks

```c
static cmark_node *add_child(cmark_parser *parser, cmark_node *parent,
                             cmark_node_type block_type, int start_column);
```

When a new block is detected, `add_child()` creates it:

1. If the parent can't contain the new block type (checked via `can_contain()`), the parent is finalized and the function moves up the tree until it finds a suitable ancestor.
2. A new node is created with `make_block()` (which sets `CMARK_NODE__OPEN`).
3. The node is linked as the last child of the parent.

### Container Acceptance Rules

```c
static inline bool can_contain(cmark_node_type parent_type,
                               cmark_node_type child_type) {
  return (parent_type == CMARK_NODE_DOCUMENT ||
          parent_type == CMARK_NODE_BLOCK_QUOTE ||
          parent_type == CMARK_NODE_ITEM ||
          (parent_type == CMARK_NODE_LIST && child_type == CMARK_NODE_ITEM));
}
```

Only documents, block quotes, list items, and lists (for items only) can contain other blocks.

## List Item Parsing

```c
static bufsize_t parse_list_marker(cmark_mem *mem, cmark_chunk *input,
                                   bufsize_t pos, bool interrupts_paragraph,
                                   cmark_list **dataptr);
```

This function detects list markers:

**Bullet markers**: `*`, `-`, or `+` followed by whitespace.

**Ordered markers**: Up to 9 digits followed by `.` or `)` and whitespace. The 9-digit limit prevents integer overflow (max value ~999,999,999 fits in a 32-bit int).

**Paragraph interruption rules**: When `interrupts_paragraph` is true (the marker would interrupt a preceding paragraph):
- Bullet markers require non-blank content after them
- Ordered markers must start at 1

### List Matching

```c
static int lists_match(cmark_list *list_data, cmark_list *item_data) {
  return (list_data->list_type == item_data->list_type &&
          list_data->delimiter == item_data->delimiter &&
          list_data->bullet_char == item_data->bullet_char);
}
```

Two list items belong to the same list only if they share the same list type, delimiter style, and bullet character. This means `- item` and `* item` create separate lists.

## Offset Advancement

```c
static void S_advance_offset(cmark_parser *parser, cmark_chunk *input,
                             bufsize_t count, bool columns);
```

This function advances `parser->offset` and `parser->column`. The `columns` parameter determines whether `count` measures bytes or virtual columns. Tab expansion is handled here:
- When counting columns and a tab appears, `chars_to_tab = TAB_STOP - (column % TAB_STOP)` determines how many columns the tab represents
- If only part of the tab is consumed (advancing fewer columns than the tab provides), `parser->partially_consumed_tab` is set

## Finding First Non-Space

```c
static void S_find_first_nonspace(cmark_parser *parser, cmark_chunk *input);
```

Scans from `parser->offset` forward, setting:
- `parser->first_nonspace` — byte position
- `parser->first_nonspace_column` — column of first non-whitespace
- `parser->indent` — `first_nonspace_column - column`
- `parser->blank` — whether the line is blank

This function is idempotent — it won't re-scan if `first_nonspace > offset`.

## Thematic Break Detection

```c
static int S_scan_thematic_break(cmark_parser *parser, cmark_chunk *input,
                                 bufsize_t offset);
```

Checks for 3 or more `*`, `_`, or `-` characters (optionally separated by spaces/tabs) on a line by themselves. Uses `parser->thematic_break_kill_pos` as an optimization to avoid re-scanning positions that already failed.

## ATX Heading Trailing Hash Removal

```c
static void chop_trailing_hashtags(cmark_chunk *ch);
```

After an ATX heading line is identified, trailing `#` characters are removed from the content if they're preceded by a space. This implements the CommonMark rule that `## Heading ##` renders as "Heading" without trailing `#` marks.

## Block Finalization

```c
static cmark_node *finalize(cmark_parser *parser, cmark_node *b);
```

When a block is closed (no longer accepting content), `finalize()` processes its accumulated content:

### Paragraph Finalization
Reference link definitions at the start are extracted:
```c
static bool resolve_reference_link_definitions(cmark_parser *parser);
```
This repeatedly calls `cmark_parse_reference_inline()` from `inlines.c` to parse reference definitions like `[label]: url "title"`. If the paragraph becomes empty after extracting all references, the paragraph node is deleted.

### Code Block Finalization
- **Fenced**: The first line becomes the info string (after HTML unescaping and trimming). Remaining content becomes the code body.
- **Indented**: Trailing blank lines are removed, and a final newline is appended.

### Heading and HTML Block Finalization
Content is simply detached from the parser's content buffer and stored in `data`.

### List Finalization
Determines tight/loose status by checking:
1. Non-final, non-empty list items ending with a blank line → loose
2. Children of list items that end with blank lines (checked recursively via `S_ends_with_blank_line()`) → loose
3. Otherwise → tight

## Document Finalization

```c
static cmark_node *finalize_document(cmark_parser *parser);
```

Called by `cmark_parser_finish()`:

1. All open blocks are finalized by walking from `parser->current` up to `parser->root`.
2. The root document is finalized.
3. Reference expansion limit is set: `refmap->max_ref_size = MAX(parser->total_size, 100000)`.
4. `process_inlines()` is called, which uses an iterator to find all nodes that contain inlines (paragraphs and headings) and calls `cmark_parse_inlines()` on each.
5. After inline parsing, the content buffer of each processed node is freed.

## Inline Content Detection

```c
static inline bool contains_inlines(cmark_node_type block_type) {
  return (block_type == CMARK_NODE_PARAGRAPH ||
          block_type == CMARK_NODE_HEADING);
}
```

Only paragraphs and headings have their string content parsed for inline elements. Code blocks, HTML blocks, and other leaf nodes preserve their content as-is.

## Lazy Continuation Lines

The CommonMark spec defines "lazy continuation lines" — lines that continue a paragraph without matching all container prefixes. For example:

```markdown
> This is a block quote
with a lazy continuation line
```

The second line doesn't start with `>` but still belongs to the paragraph inside the block quote. The parser handles this by checking whether the line could be added to an existing open paragraph rather than closing and starting a new one.

## Cross-References

- [parser.h](../../../cmark/src/parser.h) — Parser struct definition
- [blocks.c](../../../cmark/src/blocks.c) — Full implementation
- [inline-parsing.md](inline-parsing.md) — Phase 2 parsing
- [scanner-system.md](scanner-system.md) — Scanner functions used for block detection
- [reference-system.md](reference-system.md) — How reference definitions are extracted
- [ast-node-system.md](ast-node-system.md) — Node creation and tree structure
