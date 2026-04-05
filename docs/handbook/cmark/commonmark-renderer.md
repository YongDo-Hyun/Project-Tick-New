# cmark — CommonMark Renderer

## Overview

The CommonMark renderer (`commonmark.c`) converts a `cmark_node` AST back into CommonMark-formatted Markdown text. This is significantly more complex than the other renderers because it must reproduce syntactically valid Markdown that, when re-parsed, produces an equivalent AST. It uses the generic render framework from `render.c`.

## Entry Point

```c
char *cmark_render_commonmark(cmark_node *root, int options, int width);
```

- `root` — AST root node
- `options` — Option flags
- `width` — Target line width for wrapping; 0 disables wrapping

## Character Escaping (`outc`)

The CommonMark escaping is the most complex of all renderers. Three escaping modes exist:

### NORMAL Mode

Characters that could be interpreted as Markdown syntax must be backslash-escaped. Characters that trigger escaping:

```c
case '*':
case '#':
case '(':
case ')':
case '[':
case ']':
case '<':
case '>':
case '!':
case '\\':
  // Backslash-escaped: \*, \#, \(, etc.
```

Additionally:
- `.` and `)` — only escaped at line start (after a digit), to prevent triggering ordered list syntax
- `-`, `+`, `=`, `_` — only escaped at line start, to prevent thematic breaks, bullet lists, or setext headings
- `~` — only escaped at line start
- `&` — escaped to prevent entity references
- `'`, `"` — escaped for smart punctuation

For whitespace handling:
- NBSP (`\xA0`) → `\xa0` (the literal non-breaking space character)
- Tab → space (tabs cannot be reliably round-tripped)

### URL Mode

Only `(`, `)`, and whitespace `\x20` are escaped with backslashes. URLs in parenthesized `()` format need minimal escaping.

### TITLE Mode

For link titles, only the title delimiter character is escaped. The renderer currently always uses `"` as the title delimiter, so `"` is backslash-escaped within titles.

## Backtick Sequence Analysis

Two helper functions determine how to format inline code spans:

### `longest_backtick_sequence()`

```c
static int longest_backtick_sequence(const char *code) {
  int longest = 0;
  int current = 0;
  size_t i = 0;
  size_t code_len = strlen(code);
  while (i <= code_len) {
    if (code[i] == '`') {
      current++;
    } else {
      if (current > longest)
        longest = current;
      current = 0;
    }
    i++;
  }
  return longest;
}
```

Finds the maximum run of consecutive backticks within a code string.

### `shortest_unused_backtick_sequence()`

```c
static int shortest_unused_backtick_sequence(const char *code) {
  int32_t used = 1;  // Bitmask for sequences of length 1-31
  int current = 0;
  // ... scan for runs, set bits in 'used'
  int i = 0;
  while (used & 1) {
    used >>= 1;
    i++;
  }
  return i + 1;
}
```

Determines the shortest backtick sequence (1-32) that does NOT appear in the code content. This ensures the code delimiter won't conflict with backticks inside the code.

Uses a clever bit-manipulation approach: a 32-bit integer `used` tracks which backtick sequence lengths appear. After scanning, the position of the first unset bit gives the shortest unused length.

## Autolink Detection

```c
static bool is_autolink(cmark_node *node) {
  const char *title;
  const char *url;
  // ...
  if (node->first_child->type != CMARK_NODE_TEXT) return false;
  url = (char *)node->as.link.url;
  title = (char *)node->as.link.title;
  if (title && title[0]) return false;  // Autolinks have no title
  if (url &&
      (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0 ||
       strncmp(url, "mailto:", 7) == 0) &&
      strcmp(url, (char *)node->first_child->data) == 0)
    return true;
  return false;
}
```

A link is an autolink if:
1. It has exactly one child, a text node
2. No title
3. URL starts with `http://`, `https://`, or `mailto:`
4. The text exactly matches the URL

## Node Rendering (`S_render_node`)

### Block Nodes

#### Document
No output.

#### Block Quote
```
ENTER: Sets prefix to "> " for first line and "> " for continuations
EXIT:  Restores prefix, adds blank line
```

The prefix mechanism is central to CommonMark rendering. When entering a block quote:
```c
cmark_strbuf_puts(renderer->prefix, "> ");
```

All content within the block quote is prefixed with `"> "` on each line.

#### List
```
ENTER: Records tight/loose status, records bullet character
EXIT:  Restores prefix, adds blank line
```

The renderer stores whether the list is tight to control inter-item blank lines.

#### Item
```
ENTER: Computes marker and indentation prefix
EXIT:  Restores prefix
```

**Bullet items:** Use `-`, `*`, or `+` (from `cmark_node_get_list_delim`). The prefix is set to appropriate indentation:

```c
// For a bullet item:
"- "   on the first line
"  "   on continuation lines (indentation matches marker width)
```

**Ordered items:** Number is computed by counting previous siblings:
```c
list_number = cmark_node_get_list_start(node->parent);
tmp = node;
while (tmp->prev) {
  tmp = tmp->prev;
  list_number++;
}
```

Format: `"N. "` or `"N) "` depending on delimiter type. Continuation indent matches the marker width.

For tight lists, items don't emit blank lines between them.

#### Heading
**ATX headings** (levels 1-6):
```
### Content\n
```

The number of `#` characters matches the heading level. A newline follows the heading content.

**Setext headings** (levels 1-2 when `width > 0`):
Not used — the renderer always uses ATX headings.

#### Code Block
The renderer determines whether to use fenced or indented code:

**Fenced code blocks:**
```
```[info]
content
```
```

The fence character is `` ` ``. The fence length is max(3, longest_backtick_in_content + 1).

If the code has an info string, fenced blocks are always used (indented blocks cannot carry info strings).

**Indented code blocks:**
If there's no info string and `width == 0`, the renderer uses 4-space indentation by setting the prefix to `"    "`.

#### HTML Block
Content is output LITERALLY (no escaping):
```c
cmark_render_ascii(renderer, (char *)node->data);
```

This preserves raw HTML exactly.

#### Thematic Break
```
---\n
```

Uses `---` (three hyphens).

#### Paragraph
```
ENTER: (nothing for tight, blank line for normal)
EXIT:  \n (newline after content)
```

In tight lists, paragraphs don't add blank lines before/after.

### Inline Nodes

#### Text
Output with NORMAL escaping (all Markdown-significant characters escaped).

#### Soft Break
Depends on options:
- `CMARK_OPT_HARDBREAKS`: `\\\n` (backslash line break)
- `CMARK_OPT_NOBREAKS`: space
- Default: newline

#### Line Break
```
\\\n
```

Backslash followed by newline.

#### Code (inline)
The renderer selects delimiters using `shortest_unused_backtick_sequence()`:

```c
int numticks = shortest_unused_backtick_sequence(code);
// output numticks backticks
// if code starts or ends with backtick, add space padding
// output literal code
// output numticks backticks
```

If the code content starts or ends with a backtick, spaces are added inside the delimiters to prevent ambiguity:
```
`` `code` ``
```

#### Emphasis
```
ENTER: * or _     (delimiter character)
EXIT:  * or _     (matching delimiter)
```

The delimiter selection depends on what characters appear in the content. If the content contains `*`, `_` is preferred (and vice versa). The `emph_delim` variable tracks the chosen delimiter.

#### Strong
```
ENTER: ** or __
EXIT:  ** or __
```

Same delimiter selection logic as emphasis.

#### Link
**Autolinks:**
```
<URL>
```

**Normal links:**
```
ENTER: [
EXIT:  ](URL "TITLE")  or  ](URL)  if no title
```

The URL is output with URL escaping, the title with TITLE escaping.

#### Image
```
ENTER: ![
EXIT:  ](URL "TITLE")  or  ](URL)  if no title
```

Same as links but with `!` prefix.

#### HTML Inline
Output literally (no escaping).

## Prefix Management

The CommonMark renderer makes extensive use of the prefix system from `render.c`. Each line of output is prefixed with accumulated prefix strings from container nodes. For example, a list item inside a block quote:

```
> - Item text
>   continuation
```

The prefix stack would be:
1. `"> "` from the block quote
2. `"  "` (continuation indent) from the list item

The `cmark_renderer` struct maintains `prefix` and `begin_content` fields to handle this.

## Round-Trip Fidelity

The CommonMark renderer aims for round-trip fidelity: parsing the output should produce an AST equivalent to the input. This is not always perfectly achievable:

1. **Whitespace normalization**: Some whitespace differences (e.g., number of blank lines) are lost.
2. **Reference links**: Inline link syntax is always used; reference-style links are not preserved.
3. **ATX vs setext**: Always uses ATX headings.
4. **Indented vs fenced**: Logic selects one based on info string presence and width setting.
5. **Emphasis delimiter**: May differ from the original (`*` vs `_`).

## Cross-References

- [commonmark.c](../../cmark/src/commonmark.c) — Full implementation
- [render-framework.md](render-framework.md) — Generic render framework
- [public-api.md](public-api.md) — `cmark_render_commonmark()` API docs
- [scanner-system.md](scanner-system.md) — Scanners used for autolink detection
