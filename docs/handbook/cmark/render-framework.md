# cmark — Render Framework

## Overview

The render framework (`render.c`, `render.h`) provides a generic rendering infrastructure used by three of the five renderers: LaTeX, man, and CommonMark. It handles line wrapping, prefix management, and character-level output dispatch. The HTML and XML renderers bypass this framework and write directly to buffers.

## The `cmark_renderer` Structure

```c
struct cmark_renderer {
  cmark_mem *mem;
  cmark_strbuf *buffer;        // Output buffer
  cmark_strbuf *prefix;        // Current line prefix (e.g., "> " for blockquotes)
  int column;                  // Current column position (for wrapping)
  int width;                   // Target width (0 = no wrapping)
  int need_cr;                 // Pending newlines count
  bufsize_t last_breakable;    // Position of last breakable point in buffer
  bool begin_line;             // True if at the start of a line
  bool begin_content;          // True if no content has been output on current line (after prefix)
  bool no_linebreaks;          // Suppress newlines (for rendering within attributes)
  bool in_tight_list_item;     // Currently inside a tight list item
  void (*outc)(cmark_renderer *, cmark_escaping, int32_t, unsigned char);
                               // Per-character output callback
  int32_t (*render_node)(cmark_renderer *, cmark_node *, cmark_event_type, int);
                               // Per-node render callback
};
```

### Key Fields

- **`column`** — Tracks horizontal position for word-wrap decisions.
- **`width`** — If > 0, enables automatic line wrapping at word boundaries.
- **`prefix`** — Accumulated prefix string. For nested block quotes and list items, prefixes stack (e.g., `"> - "` for a list item inside a block quote).
- **`last_breakable`** — Buffer position of the last whitespace where a line break could be inserted. Used for retroactive line wrapping.
- **`begin_line`** — True immediately after a newline. Used by renderers to decide whether to escape line-start characters.
- **`begin_content`** — True until the first non-prefix content on a line. Distinguished from `begin_line` because the prefix itself isn't "content".
- **`no_linebreaks`** — When true, newlines are converted to spaces. Used when rendering content inside constructs that can't contain literal newlines.

## Entry Point

```c
char *cmark_render(cmark_mem *mem, cmark_node *root, int options, int width,
                   void (*outc)(cmark_renderer *, cmark_escaping, int32_t, unsigned char),
                   int32_t (*render_node)(cmark_renderer *, cmark_node *,
                                         cmark_event_type, int)) {
  cmark_renderer renderer = {
    mem,
    &buf,     // buffer
    &pref,    // prefix
    0,        // column
    width,    // width
    0,        // need_cr
    0,        // last_breakable
    true,     // begin_line
    true,     // begin_content
    false,    // no_linebreaks
    false,    // in_tight_list_item
    outc,     // outc
    render_node  // render_node
  };
  // ... iterate AST, call render_node for each event
  return (char *)cmark_strbuf_detach(&buf);
}
```

The framework creates a `cmark_renderer`, iterates over the AST using `cmark_iter`, and calls the provided `render_node` function for each event. The `outc` callback handles per-character output with escaping decisions.

## Escaping Modes

```c
typedef enum {
  LITERAL,   // No escaping — output characters as-is
  NORMAL,    // Full escaping for prose text
  TITLE,     // Escaping for link titles
  URL,       // Escaping for URLs
} cmark_escaping;
```

Each renderer's `outc` function switches on this enum to determine how to handle special characters.

## Output Functions

### `cmark_render_code_point()`

```c
void cmark_render_code_point(cmark_renderer *renderer, int32_t c) {
  cmark_utf8proc_encode_char(c, renderer->buffer);
  renderer->column += 1;
}
```

Low-level: encodes a single Unicode codepoint as UTF-8 into the buffer and advances the column counter.

### `cmark_render_ascii()`

```c
void cmark_render_ascii(cmark_renderer *renderer, const char *s) {
  int len = (int)strlen(s);
  cmark_strbuf_puts(renderer->buffer, s);
  renderer->column += len;
}
```

Outputs an ASCII string and advances the column counter. Used for fixed escape sequences like `\&`, `\textbf{`, etc.

### `S_out()` — Main Output Dispatcher

```c
static CMARK_INLINE void S_out(cmark_renderer *renderer, const char *source,
                                bool wrap, cmark_escaping escape) {
  int length = (int)strlen(source);
  unsigned char nextc;
  int32_t c;
  int i = 0;
  int len;
  cmark_chunk remainder = cmark_chunk_literal("");
  int k = renderer->buffer->size - 1;

  wrap = wrap && !renderer->no_linebreaks;

  if (renderer->need_cr) {
    // Output pending newlines
    while (renderer->need_cr > 0) {
      S_cr(renderer);
      renderer->need_cr--;
    }
  }

  while (i < length) {
    if (renderer->begin_line) {
      // Output prefix at start of each line
      cmark_strbuf_puts(renderer->buffer, (char *)renderer->prefix->ptr);
      renderer->column = renderer->prefix->size;
      renderer->begin_line = false;
      renderer->begin_content = true;
    }

    len = cmark_utf8proc_charlen((uint8_t *)source + i, length - i);
    if (len == -1) { // Invalid UTF-8
      // ... handle error
    }

    cmark_utf8proc_iterate((uint8_t *)source + i, len, &c);

    if (c == 10) {
      // Newline
      cmark_strbuf_putc(renderer->buffer, '\n');
      renderer->column = 0;
      renderer->begin_line = true;
      renderer->begin_content = true;
      renderer->last_breakable = 0;
    } else if (wrap) {
      if (c == 32 && renderer->column > renderer->width / 2) {
        // Space past half-width — mark as potential break point
        renderer->last_breakable = renderer->buffer->size;
        cmark_render_code_point(renderer, c);
      } else if (renderer->column > renderer->width &&
                 renderer->last_breakable > 0) {
        // Past target width with a break point — retroactively break
        // Replace the space at last_breakable with newline + prefix
        // ...
      } else {
        renderer->outc(renderer, escape, c, nextc);
      }
    } else {
      renderer->outc(renderer, escape, c, nextc);
    }

    if (c != 10) {
      renderer->begin_content = false;
    }
    i += len;
  }
}
```

This is the core output function. It:
1. Handles deferred newlines (`need_cr`)
2. Outputs line prefixes at the start of each line
3. Tracks column position
4. Implements word wrapping via retroactive line breaks
5. Delegates character-level escaping to `renderer->outc()`

### Line Wrapping Algorithm

The wrapping algorithm uses a **retroactive break** strategy:

1. As text flows through `S_out()`, spaces past the half-width mark are recorded as potential break points (`last_breakable`).
2. When the column exceeds `width`, the buffer is split at `last_breakable`:
   - Everything after the break point is saved in `remainder`
   - A newline and the current prefix are inserted at the break point
   - The remainder is reappended

This avoids forward-looking: the renderer doesn't need to know the length of upcoming content to decide where to break.

```c
// Retroactive line break:
remainder = cmark_chunk_dup(&renderer->buffer->..., last_breakable, ...);
cmark_strbuf_truncate(renderer->buffer, last_breakable);
cmark_strbuf_putc(renderer->buffer, '\n');
cmark_strbuf_puts(renderer->buffer, (char *)renderer->prefix->ptr);
cmark_strbuf_put(renderer->buffer, remainder.data, remainder.len);
renderer->column = renderer->prefix->size + cmark_chunk_len(&remainder);
renderer->last_breakable = 0;
renderer->begin_line = false;
renderer->begin_content = false;
```

## Convenience Functions

### `CR()`

```c
#define CR() renderer->need_cr = 1
```

Requests a newline before the next content output. Multiple `CR()` calls don't stack — only one newline is inserted.

### `BLANKLINE()`

```c
#define BLANKLINE() renderer->need_cr = 2
```

Requests a blank line (two newlines) before the next content output.

### `OUT()`

```c
#define OUT(s, wrap, escaping) (S_out(renderer, s, wrap, escaping))
```

### `LIT()`

```c
#define LIT(s) (S_out(renderer, s, false, LITERAL))
```

Output literal text (no escaping, no wrapping).

### `NOBREAKS()`

```c
#define NOBREAKS(s) \
  do { renderer->no_linebreaks = true; OUT(s, false, NORMAL); renderer->no_linebreaks = false; } while(0)
```

Output text with normal escaping but with newlines suppressed (converted to spaces).

## Prefix Management

Prefixes are used for block-level indentation. The renderer maintains a `cmark_strbuf` prefix that is output at the start of each line.

### Usage Pattern

```c
// In commonmark.c, entering a block quote:
cmark_strbuf_puts(renderer->prefix, "> ");
// ... render children ...
// On exit:
cmark_strbuf_truncate(renderer->prefix, original_prefix_len);
```

Renderers save the prefix length before modifying it and restore it on exit. This creates a stack-like behavior for nested containers.

## Framework vs Direct Rendering

| Feature | Framework (render.c) | Direct (html.c, xml.c) |
|---------|---------------------|----------------------|
| Line wrapping | Yes (`width` parameter) | No |
| Prefix management | Yes (automatic) | No (uses HTML tags) |
| Per-char escaping | Via `outc` callback | Via `escape_html()` helper |
| Column tracking | Yes | No |
| Break points | Retroactive insertion | N/A |
| `cmark_escaping` enum | Yes | No |

## Which Renderers Use the Framework

| Renderer | Uses Framework | Why/Why Not |
|----------|---------------|-------------|
| LaTeX (`latex.c`) | Yes | Needs wrapping for structured text |
| man (`man.c`) | Yes | Needs wrapping for terminal display |
| CommonMark (`commonmark.c`) | Yes | Needs wrapping and prefix management |
| HTML (`html.c`) | No | HTML handles layout via browser |
| XML (`xml.c`) | No | XML output is structural, not visual |

## Cross-References

- [render.c](../../cmark/src/render.c) — Framework implementation
- [render.h](../../cmark/src/render.h) — `cmark_renderer` struct and `cmark_escaping` enum
- [latex-renderer.md](latex-renderer.md) — LaTeX `outc` and `S_render_node`
- [man-renderer.md](man-renderer.md) — Man `S_outc` and `S_render_node`
- [commonmark-renderer.md](commonmark-renderer.md) — CommonMark `outc` and `S_render_node`
- [html-renderer.md](html-renderer.md) — Direct renderer (no framework)
