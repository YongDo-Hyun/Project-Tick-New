# cmark — Man Page Renderer

## Overview

The man page renderer (`man.c`) converts a `cmark_node` AST into roff/troff format suitable for the Unix `man` page system. It uses the generic render framework from `render.c`.

## Entry Point

```c
char *cmark_render_man(cmark_node *root, int options, int width);
```

- `root` — AST root node
- `options` — Option flags (`CMARK_OPT_HARDBREAKS`, `CMARK_OPT_NOBREAKS`, `CMARK_OPT_SOURCEPOS`, `CMARK_OPT_UNSAFE`)
- `width` — Target line width for wrapping; 0 disables wrapping

## Character Escaping (`S_outc`)

The man page escaping is simpler than LaTeX. The `S_outc` function handles:

```c
static void S_outc(cmark_renderer *renderer, cmark_escaping escape,
                   int32_t c, unsigned char nextc) {
  if (escape == LITERAL) {
    cmark_render_code_point(renderer, c);
    return;
  }
  switch (c) {
  case 46:   // '.'  — if at line start
    cmark_render_ascii(renderer, "\\&.");
    break;
  case 39:   // '\'' — if at line start
    cmark_render_ascii(renderer, "\\&'");
    break;
  case 45:   // '-'
    cmark_render_ascii(renderer, "\\-");
    break;
  case 92:   // '\\'
    cmark_render_ascii(renderer, "\\e");
    break;
  case 8216: // '  (left single quote)
    cmark_render_ascii(renderer, "\\[oq]");
    break;
  case 8217: // '  (right single quote)
    cmark_render_ascii(renderer, "\\[cq]");
    break;
  case 8220: // "  (left double quote)
    cmark_render_ascii(renderer, "\\[lq]");
    break;
  case 8221: // "  (right double quote)
    cmark_render_ascii(renderer, "\\[rq]");
    break;
  case 8212: // —  (em dash)
    cmark_render_ascii(renderer, "\\[em]");
    break;
  case 8211: // –  (en dash)
    cmark_render_ascii(renderer, "\\[en]");
    break;
  default:
    cmark_render_code_point(renderer, c);
  }
}
```

### Line-Start Protection

The `.` and `'` characters are only escaped when they appear at the beginning of a line, since roff interprets them as macro/command prefixes. The check:

```c
case 46:
case 39:
  if (renderer->begin_line) {
    cmark_render_ascii(renderer, "\\&.");  // or "\\&'"
  }
```

The `\\&` prefix is a zero-width space that prevents roff from treating the character as a command prefix.

## Block Number Tracking

The renderer tracks nesting with a `block_number` variable for generating matching `.RS`/`.RE` (indent start/end) pairs:

This variable is incremented when entering list items and block quotes, and decremented on exit. It controls the indentation level of nested content.

## Node Rendering (`S_render_node`)

### Block Nodes

#### Document
No output on enter or exit.

#### Block Quote
```
ENTER: .RS\n
EXIT:  .RE\n
```

`.RS` pushes relative indentation, `.RE` pops it.

#### List
On exit, adds a blank output line (`cr()`) to separate from following content.

#### Item
```
ENTER (bullet):  .IP \(bu 2\n
ENTER (ordered): .IP "N." 4\n    (where N = list start + sibling count)
EXIT:            (cr if not last item)
```

The ordered item number is calculated by counting previous siblings:
```c
int list_number = cmark_node_get_list_start(node->parent);
tmp = node;
while (tmp->prev) {
  tmp = tmp->prev;
  list_number++;
}
```

`.IP` sets an indented paragraph with a tag (bullet or number) and indentation width.

#### Heading
```
ENTER (level 1): .SH\n      (section heading)
ENTER (level 2): .SS\n      (subsection heading)
ENTER (other):   .PP\n\fB   (paragraph, start bold)
EXIT  (other):   \fR\n      (end bold)
```

Level 1 and 2 headings use dedicated roff macros. Level 3+ are rendered as bold paragraphs.

#### Code Block
```
.IP\n.nf\n\\f[C]\n
LITERAL CONTENT
\\f[]\n.fi\n
```

- `.nf` — no-fill (preformatted)
- `\\f[C]` — switch to constant-width font
- `\\f[]` — restore previous font
- `.fi` — return to fill mode

#### HTML Block
```
(nothing)
```
Raw HTML blocks are silently omitted in man output.

#### Thematic Break
There is no native roff thematic break. The renderer outputs nothing for this node type.

#### Paragraph
Same tight-list check as other renderers:
```c
tight = (grandparent && grandparent->type == CMARK_NODE_LIST) ?
        grandparent->as.list.tight : false;
```
- Normal: `.PP\n` before content
- Tight: no `.PP` prefix

### Inline Nodes

#### Text
Output with NORMAL escaping.

#### Soft Break
Depends on options:
- `CMARK_OPT_HARDBREAKS`: `.PD 0\n.P\n.PD\n`
- `CMARK_OPT_NOBREAKS`: space
- Default: newline

The hardbreak sequence `.PD 0\n.P\n.PD\n` is a man page idiom that:
1. Sets paragraph distance to 0 (`.PD 0`)
2. Starts a new paragraph (`.P`)
3. Restores default paragraph distance (`.PD`)

#### Line Break
Same as hardbreak:
```
.PD 0\n.P\n.PD\n
```

#### Code (inline)
```
\f[C]ESCAPED CONTENT\f[]
```

Font switch to `C` (constant-width), then restore.

#### Emphasis
```
ENTER: \f[I]    (italic font)
EXIT:  \f[]     (restore font)
```

#### Strong
```
ENTER: \f[B]    (bold font)
EXIT:  \f[]     (restore font)
```

#### Link
Links render their text content normally. On exit:
```
(ESCAPED_URL)
```

If the link URL is the same as the text content (autolink), the URL suffix is suppressed.

#### Image
```
ENTER: [IMAGE: 
EXIT:  ]
```

Images have no roff equivalent, so they're rendered as bracketed alt text.

#### HTML Inline
Silently omitted.

## Source Position

When `CMARK_OPT_SOURCEPOS` is set, man output includes roff comments:
```
.\" sourcepos: LINE:COL-LINE:COL
```

(The `.\"` prefix is the roff comment syntax.)

## Example Output

Markdown input:
```markdown
# My Tool

A description with *emphasis*.

## Options

- `--flag` — Does something
- `--other` — Does another thing
```

Man output:
```roff
.SH
My Tool
.PP
A description with \f[I]emphasis\f[].
.SS
Options
.IP \(bu 2
\f[C]\-\-flag\f[] \[em] Does something
.IP \(bu 2
\f[C]\-\-other\f[] \[em] Does another thing
```

## Limitations

1. **No heading levels > 2**: Levels 3+ are rendered as bold paragraphs, losing semantic heading structure.
2. **No images**: Only alt text is shown in brackets.
3. **No raw HTML**: Silently dropped.
4. **No thematic breaks**: No visual separator is output.
5. **No tables**: Not part of core CommonMark, but if extensions add them, the man renderer has no support.

## Cross-References

- [man.c](../../cmark/src/man.c) — Full implementation
- [render-framework.md](render-framework.md) — Generic render framework
- [public-api.md](public-api.md) — `cmark_render_man()` API docs
- [latex-renderer.md](latex-renderer.md) — Another framework-based renderer
