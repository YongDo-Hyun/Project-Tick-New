# cmark — Scanner System

## Overview

The scanner system (`scanners.h`, `scanners.re`, `scanners.c`) provides fast pattern-matching functions used throughout cmark's block and inline parsers. The scanners are generated from re2c specifications and compiled into optimized C switch-statement automata. They perform context-free matching only (no backtracking, no captures beyond match length).

## Architecture

### Source Files

- `scanners.re` — re2c source with pattern specifications
- `scanners.c` — Generated C code (committed to the repository, regenerated manually)
- `scanners.h` — Public declarations (macro wrappers and function prototypes)

### Generation

Scanners are regenerated from re2c source via:
```bash
re2c --case-insensitive -b -i --no-generation-date --8bit -o scanners.c scanners.re
```

Flags:
- `--case-insensitive` — Case-insensitive matching
- `-b` — Use bit vectors for character classes
- `-i` — Use `if` statements instead of `switch`
- `--no-generation-date` — Reproducible output
- `--8bit` — 8-bit character width

The generated code consists of state machines implemented as nested `switch`/`if` blocks with direct character comparisons. There are no regular expression structs, no DFA tables — the patterns are compiled directly into C control flow.

## Scanner Interface

### The `_scan_at` Wrapper

```c
#define _scan_at(scanner, s, p) scanner(s->input.data, s->input.len, p)
```

All scanner functions share the signature:
```c
bufsize_t scan_PATTERN(const unsigned char *s, bufsize_t len, bufsize_t offset);
```

Parameters:
- `s` — Input byte string
- `len` — Total length of `s`
- `offset` — Starting position within `s`

Return value:
- Length of the match (in bytes) if successful
- `0` if no match at the given position

### Common Pattern

```c
// In blocks.c:
matched = _scan_at(&scan_thematic_break, &input, first_nonspace);

// In inlines.c:
matched = _scan_at(&scan_autolink_uri, subj, subj->pos);
```

## Scanner Functions

### Block Structure Scanners

| Scanner | Purpose | Used In |
|---------|---------|---------|
| `scan_thematic_break` | Matches `***`, `---`, `___` (with optional spaces) | `blocks.c` |
| `scan_atx_heading_start` | Matches `#{1,6}` followed by space or EOL | `blocks.c` |
| `scan_setext_heading_line` | Matches `=+` or `-+` at line start | `blocks.c` |
| `scan_open_code_fence` | Matches `` ``` `` or `~~~` (3+ fence chars) | `blocks.c` |
| `scan_close_code_fence` | Matches closing fence (≥ opening length) | `blocks.c` |
| `scan_html_block_start` | Matches HTML block type 1-5 openers | `blocks.c` |
| `scan_html_block_start_7` | Matches HTML block type 6-7 openers | `blocks.c` |
| `scan_html_block_end_1` | Matches `</script>`, `</pre>`, `</style>` | `blocks.c` |
| `scan_html_block_end_2` | Matches `-->` | `blocks.c` |
| `scan_html_block_end_3` | Matches `?>` | `blocks.c` |
| `scan_html_block_end_4` | Matches `>` | `blocks.c` |
| `scan_html_block_end_5` | Matches `]]>` | `blocks.c` |
| `scan_link_title` | Matches `"..."`, `'...'`, or `(...)` titles | `inlines.c` |

### Inline Scanners

| Scanner | Purpose | Used In |
|---------|---------|---------|
| `scan_autolink_uri` | Matches URI autolinks `<scheme:path>` | `inlines.c` |
| `scan_autolink_email` | Matches email autolinks `<user@host>` | `inlines.c` |
| `scan_html_tag` | Matches inline HTML tags (open, close, comment, PI, CDATA, declaration) | `inlines.c` |
| `scan_entity` | Matches HTML entities (`&amp;`, `&#123;`, `&#x1F;`) | `inlines.c` |
| `scan_dangerous_url` | Matches `javascript:`, `vbscript:`, `file:`, `data:` URLs | `html.c` |
| `scan_spacechars` | Matches runs of spaces and tabs | `inlines.c` |

### Link/Reference Scanners

| Scanner | Purpose | Used In |
|---------|---------|---------|
| `scan_link_url` | Matches link destinations (parenthesized or bare) | `inlines.c` |
| `scan_link_title` | Matches quoted link titles | `inlines.c` |

## Scanner Patterns (from `scanners.re`)

### Thematic Break
```
thematic_break = (('*' [ \t]*){3,} | ('-' [ \t]*){3,} | ('_' [ \t]*){3,}) [ \t]* [\n]
```
Three or more `*`, `-`, or `_` characters, optionally separated by spaces/tabs.

### ATX Heading
```
atx_heading_start = '#{1,6}' ([ \t]+ | [\n])
```
1-6 `#` characters followed by space/tab or newline.

### Code Fence
```
open_code_fence = '`{3,}' [^`\n]* [\n] | '~{3,}' [^\n]* [\n]
```
Three or more backticks (not followed by backtick in info string) or three or more tildes.

### HTML Block Start (Types 1-7)

The CommonMark spec defines 7 types of HTML blocks, each matched by different scanners:

1. `<script>`, `<pre>`, `<style>` (case-insensitive)
2. `<!--`
3. `<?`
4. `<!` followed by uppercase letter (declaration)
5. `<![CDATA[`
6. HTML tags from a specific set (e.g., `<div>`, `<table>`, `<h1>`, etc.)
7. Complete open/close tags (not `<script>`, `<pre>`, `<style>`)

### Autolink URI
```
autolink_uri = '<' scheme ':' [^\x00-\x20<>]* '>'
scheme = [A-Za-z][A-Za-z0-9+.\-]{1,31}
```

### Autolink Email
```
autolink_email = '<' [A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+ '@'
                 [A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?
                 ('.' [A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)* '>'
```

### HTML Entity
```
entity = '&' ('#' ('x'|'X') [0-9a-fA-F]{1,6} | '#' [0-9]{1,7} | [A-Za-z][A-Za-z0-9]{1,31}) ';'
```

### Dangerous URL
```
dangerous_url = ('javascript' | 'vbscript' | 'file' | 'data'
                 (not followed by image MIME types)) ':'
```

Data URLs are allowed if followed by `image/png`, `image/gif`, `image/jpeg`, or `image/webp`.

### HTML Tag
```
html_tag = open_tag | close_tag | html_comment | processing_instruction | declaration | cdata
open_tag = '<' tag_name attribute* '/' ? '>'
close_tag = '</' tag_name [ \t]* '>'
html_comment = '<!--' ...
processing_instruction = '<?' ...
declaration = '<!' [A-Z]+ ...
cdata = '<![CDATA[' ...
```

## Generated Code Structure

The generated `scanners.c` contains functions like:

```c
bufsize_t _scan_thematic_break(const unsigned char *p, bufsize_t len,
                               bufsize_t offset) {
  const unsigned char *marker = NULL;
  const unsigned char *start = p + offset;
  // ... re2c-generated state machine
  // Returns (bufsize_t)(p - start) on match, 0 on failure
}
```

Each function is a self-contained state machine that:
1. Starts at `p + offset`
2. Walks forward byte-by-byte through the pattern
3. Returns the match length or 0

The generated code is typically hundreds of lines per scanner function, with deeply nested `if`/`switch` chains for the character transitions.

## Performance Characteristics

- **O(n)** in the length of the match — each scanner reads input exactly once
- **No backtracking** — re2c generates DFA-based scanners
- **No allocation** — scanners work on existing buffers, no heap allocation
- **Branch prediction friendly** — the common case (no match) typically hits the first branch

## Usage Example

A typical block-parsing sequence using scanners:

```c
// Check if line starts a thematic break
if (!indented &&
    (input.data[first_nonspace] == '*' ||
     input.data[first_nonspace] == '-' ||
     input.data[first_nonspace] == '_')) {
  matched = _scan_at(&scan_thematic_break, &input, first_nonspace);
  if (matched) {
    // Create thematic break node
  }
}
```

The manual character check before calling the scanner is an optimization — it avoids the function call overhead when the first character can't possibly start the pattern.

## Cross-References

- [scanners.h](../../cmark/src/scanners.h) — Scanner declarations and `_scan_at` macro
- [scanners.re](../../cmark/src/scanners.re) — re2c source (if available)
- [block-parsing.md](block-parsing.md) — Block-level scanner usage
- [inline-parsing.md](inline-parsing.md) — Inline scanner usage
- [html-renderer.md](html-renderer.md) — `scan_dangerous_url()` for URL safety
