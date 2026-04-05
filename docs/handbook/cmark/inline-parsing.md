# cmark — Inline Parsing

## Overview

Inline parsing is Phase 2 of cmark's pipeline. Implemented in `inlines.c`, it processes the text content of paragraph and heading nodes, recognizing emphasis (`*`, `_`), code spans (`` ` ``), links (`[text](url)`), images (`![alt](url)`), autolinks (`<url>`), raw HTML inline, hard line breaks, soft line breaks, and smart punctuation.

The entry point is `cmark_parse_inlines()`, called from `process_inlines()` in `blocks.c` after all block structure has been finalized.

## The `subject` Struct

All inline parsing state is tracked in the `subject` struct:

```c
typedef struct {
  cmark_mem *mem;                          // Memory allocator
  cmark_chunk input;                       // The text being parsed
  unsigned flags;                          // Skip flags for HTML constructs
  int line;                                // Source line number
  bufsize_t pos;                           // Current byte position in input
  int block_offset;                        // Column offset of the containing block
  int column_offset;                       // Adjustment for multi-line source position tracking
  cmark_reference_map *refmap;             // Link reference definitions
  delimiter *last_delim;                   // Top of delimiter stack (linked list, newest first)
  bracket *last_bracket;                   // Top of bracket stack (linked list, newest first)
  bufsize_t backticks[MAXBACKTICKS + 1];   // Cached positions of backtick sequences
  bool scanned_for_backticks;              // Whether the full input has been scanned for backticks
  bool no_link_openers;                    // Optimization: set when no link openers remain
} subject;
```

`MAXBACKTICKS` is defined as 1000. The `backticks` array caches the positions of backtick sequences of each length, enabling O(1) lookup once the input has been fully scanned.

### Skip Flags

The `flags` field uses bit flags to track which HTML constructs have been confirmed absent:

```c
#define FLAG_SKIP_HTML_CDATA        (1u << 0)
#define FLAG_SKIP_HTML_DECLARATION  (1u << 1)
#define FLAG_SKIP_HTML_PI           (1u << 2)
#define FLAG_SKIP_HTML_COMMENT      (1u << 3)
```

Once a scan for a particular HTML construct fails, the flag is set to avoid rescanning.

## The Delimiter Stack

Emphasis and smart punctuation use a delimiter stack. Each entry is:

```c
typedef struct delimiter {
  struct delimiter *previous;    // Link to older delimiter
  struct delimiter *next;        // Link to newer delimiter (towards top)
  cmark_node *inl_text;          // The text node created for this delimiter run
  bufsize_t position;            // Position in the input
  bufsize_t length;              // Number of delimiter characters remaining
  unsigned char delim_char;      // '*', '_', '\'', or '"'
  bool can_open;                 // Whether this run can open emphasis
  bool can_close;                // Whether this run can close emphasis
} delimiter;
```

The stack is a doubly-linked list with `last_delim` pointing to the newest entry.

## The Bracket Stack

Links and images use a separate bracket stack:

```c
typedef struct bracket {
  struct bracket *previous;      // Link to older bracket
  cmark_node *inl_text;          // The text node for '[' or '!['
  bufsize_t position;            // Position in the input
  bool image;                    // Whether this is an image opener '!['
  bool active;                   // Can still match (set to false when deactivated)
  bool bracket_after;            // Whether a '[' appeared after this bracket
} bracket;
```

Brackets are deactivated (set `active = false`) when:
- A matching `]` fails to produce a valid link (the opener is deactivated to prevent infinite loops)
- An inner link is formed (outer brackets are deactivated per spec)

## Emphasis Flanking Rules: `scan_delims()`

```c
static int scan_delims(subject *subj, unsigned char c, bool *can_open,
                       bool *can_close);
```

This function determines whether a run of `*`, `_`, `'`, or `"` characters can open and/or close emphasis, following the CommonMark spec's Unicode-aware flanking rules:

1. The function looks at the character **before** the run and the character **after** the run.
2. It uses `cmark_utf8proc_iterate()` to decode the surrounding characters as full Unicode code points.
3. It classifies them using `cmark_utf8proc_is_space()` and `cmark_utf8proc_is_punctuation_or_symbol()`.

The flanking rules:
- **Left-flanking**: numdelims > 0, character after is not a space, AND (character after is not punctuation OR character before is a space or punctuation)
- **Right-flanking**: numdelims > 0, character before is not a space, AND (character before is not punctuation OR character after is a space or punctuation)

For `*`: `can_open = left_flanking`, `can_close = right_flanking`

For `_`:
```c
*can_open = left_flanking &&
            (!right_flanking || cmark_utf8proc_is_punctuation_or_symbol(before_char));
*can_close = right_flanking &&
             (!left_flanking || cmark_utf8proc_is_punctuation_or_symbol(after_char));
```

For `'` and `"` (smart punctuation):
```c
*can_open = left_flanking &&
     (!right_flanking || before_char == '(' || before_char == '[') &&
     before_char != ']' && before_char != ')';
*can_close = right_flanking;
```

The function advances `subj->pos` past the delimiter run and returns the number of delimiter characters consumed. For quotes, only 1 delimiter is consumed regardless of how many appear.

## Emphasis Resolution: `S_insert_emph()`

```c
static delimiter *S_insert_emph(subject *subj, delimiter *opener,
                                delimiter *closer);
```

When a closing delimiter is found that matches an opener on the stack, this function creates emphasis nodes:

1. If the opener and closer have combined length >= 2 AND both have individual length >= 2, create a `CMARK_NODE_STRONG` node (consuming 2 characters from each).
2. Otherwise, create a `CMARK_NODE_EMPH` node (consuming 1 character from each).
3. All inline nodes between the opener and closer are moved to become children of the new emphasis node.
4. Any delimiters between the opener and closer are removed from the stack.
5. If the opener is exhausted (`length == 0`), it's removed from the stack.
6. If the closer is exhausted, it's removed too; otherwise, processing continues.

## Code Span Parsing: `handle_backticks()`

```c
static cmark_node *handle_backticks(subject *subj, int options);
```

When a backtick is encountered:

1. `take_while(subj, isbacktick)` consumes the opening backtick run and records its length.
2. `scan_to_closing_backticks()` searches forward for a matching backtick run of the same length.

The scanning function uses the `subj->backticks[]` array to cache positions of backtick sequences. If `subj->scanned_for_backticks` is true and the cached position for the needed length is behind the current position, it immediately returns 0 (no match).

If no closing backticks are found, the opening run is emitted as literal text. If found, the content between is extracted, normalized via `S_normalize_code()`:

```c
static void S_normalize_code(cmark_strbuf *s) {
  // 1. Convert \r\n and \r to spaces
  // 2. Convert \n to spaces
  // 3. If content begins and ends with a space and contains non-space chars,
  //    strip one leading and one trailing space
}
```

## Link Parsing

When `]` is encountered after an opener on the bracket stack:

### Inline Links: `[text](url "title")`

The parser looks for `(` immediately after `]`, then:
1. Skips optional whitespace
2. Tries to parse a link destination (URL)
3. Skips optional whitespace
4. Optionally parses a link title (in single quotes, double quotes, or parentheses)
5. Expects `)`

### Reference Links: `[text][ref]` or `[text][]` or `[text]`

If the inline link syntax doesn't match, the parser tries:
1. `[text][ref]` — explicit reference
2. `[text][]` — collapsed reference (label = text)
3. `[text]` — shortcut reference (label = text)

Reference lookup uses `cmark_reference_lookup()` against the parser's `refmap`.

### URL Cleaning

```c
unsigned char *cmark_clean_url(cmark_mem *mem, cmark_chunk *url);
```

Trims the URL, unescapes HTML entities, and handles angle-bracket-delimited URLs.

### Autolinks

```c
static inline cmark_node *make_autolink(subject *subj, int start_column,
                                        int end_column, cmark_chunk url,
                                        int is_email);
```

Autolinks (`<http://example.com>` or `<user@example.com>`) are detected via the `scan_autolink_uri()` and `scan_autolink_email()` scanner functions. Email autolinks have `mailto:` prepended to the URL automatically:

```c
static unsigned char *cmark_clean_autolink(cmark_mem *mem, cmark_chunk *url,
                                           int is_email) {
  cmark_strbuf buf = CMARK_BUF_INIT(mem);
  cmark_chunk_trim(url);
  if (is_email)
    cmark_strbuf_puts(&buf, "mailto:");
  houdini_unescape_html_f(&buf, url->data, url->len);
  return cmark_strbuf_detach(&buf);
}
```

## Smart Punctuation

When `CMARK_OPT_SMART` is enabled, the inline parser transforms:

```c
static const char *EMDASH = "\xE2\x80\x94";           // —
static const char *ENDASH = "\xE2\x80\x93";           // –
static const char *ELLIPSES = "\xE2\x80\xA6";         // …
static const char *LEFTDOUBLEQUOTE = "\xE2\x80\x9C";  // "
static const char *RIGHTDOUBLEQUOTE = "\xE2\x80\x9D"; // "
static const char *LEFTSINGLEQUOTE = "\xE2\x80\x98";  // '
static const char *RIGHTSINGLEQUOTE = "\xE2\x80\x99";  // '
```

- `---` becomes em dash (—)
- `--` becomes en dash (–)
- `...` becomes ellipsis (…)
- `'` and `"` are converted to curly quotes using the delimiter stack (open/close logic)

## Hard and Soft Line Breaks

- **Hard line break**: Two or more spaces before a line ending, or a backslash before a line ending. Creates a `CMARK_NODE_LINEBREAK` node.
- **Soft line break**: A line ending not preceded by spaces or backslash. Creates a `CMARK_NODE_SOFTBREAK` node.

## Special Character Dispatch

```c
static bufsize_t subject_find_special_char(subject *subj, int options);
```

This function scans forward from `subj->pos` looking for the next special character that needs inline processing. Special characters include:
- Line endings (`\r`, `\n`)
- Backtick (`` ` ``)
- Backslash (`\`)
- Ampersand (`&`)
- Less-than (`<`)
- Open bracket (`[`)
- Close bracket (`]`)
- Exclamation mark (`!`)
- Emphasis characters (`*`, `_`)

Any text between special characters is collected as a `CMARK_NODE_TEXT` node.

## Source Position Tracking

```c
static void adjust_subj_node_newlines(subject *subj, cmark_node *node,
                                     int matchlen, int extra, int options);
```

When `CMARK_OPT_SOURCEPOS` is enabled, this function adjusts source positions for multi-line inline constructs. It counts newlines in the just-matched span and updates:
- `subj->line` — incremented by the number of newlines
- `node->end_line` — adjusted for multi-line spans
- `node->end_column` — set to characters after the last newline
- `subj->column_offset` — adjusted for correct subsequent position calculations

## Inline Node Factory Functions

The inline parser uses efficient factory functions:

```c
// Macros for simple nodes
#define make_linebreak(mem) make_simple(mem, CMARK_NODE_LINEBREAK)
#define make_softbreak(mem) make_simple(mem, CMARK_NODE_SOFTBREAK)
#define make_emph(mem) make_simple(mem, CMARK_NODE_EMPH)
#define make_strong(mem) make_simple(mem, CMARK_NODE_STRONG)
```

```c
// Fast child appending (bypasses S_can_contain validation)
static void append_child(cmark_node *node, cmark_node *child) {
  cmark_node *old_last_child = node->last_child;
  child->next = NULL;
  child->prev = old_last_child;
  child->parent = node;
  node->last_child = child;
  if (old_last_child) {
    old_last_child->next = child;
  } else {
    node->first_child = child;
  }
}
```

This `append_child()` is a simplified version of the public `cmark_node_append_child()`, skipping containership validation since the inline parser always produces valid structures.

## The Main Parse Loop

```c
void cmark_parse_inlines(cmark_mem *mem, cmark_node *parent,
                         cmark_reference_map *refmap, int options);
```

This function initializes a `subject` from the parent node's `data` field, then repeatedly calls `parse_inline()` until the input is exhausted. Each call to `parse_inline()` finds the next special character, emits any preceding text as a `CMARK_NODE_TEXT`, and dispatches to the appropriate handler.

After all characters are processed, the delimiter stack is processed to resolve any remaining emphasis, and then cleaned up.

## Cross-References

- [inlines.c](../../cmark/src/inlines.c) — Full implementation
- [inlines.h](../../cmark/src/inlines.h) — Internal API declarations
- [block-parsing.md](block-parsing.md) — Phase 1 that produces the input for inline parsing
- [reference-system.md](reference-system.md) — How link references are stored and looked up
- [scanner-system.md](scanner-system.md) — Scanner functions for HTML tags, autolinks, etc.
- [utf8-handling.md](utf8-handling.md) — Unicode character classification for flanking rules
