# cmark — HTML Renderer

## Overview

The HTML renderer (`html.c`) converts a `cmark_node` AST into an HTML string. Unlike the LaTeX, man, and CommonMark renderers, it does NOT use the generic render framework from `render.c`. Instead, it writes directly to a `cmark_strbuf` buffer, giving it full control over output formatting.

## Entry Point

```c
char *cmark_render_html(cmark_node *root, int options);
```

Creates an iterator over the AST, processes each node via `S_render_node()`, and returns the resulting HTML string. The caller is responsible for freeing the returned buffer.

### Implementation

```c
char *cmark_render_html(cmark_node *root, int options) {
  char *result;
  cmark_strbuf html = CMARK_BUF_INIT(root->mem);
  cmark_event_type ev_type;
  cmark_node *cur;
  struct render_state state = {&html, NULL};
  cmark_iter *iter = cmark_iter_new(root);

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    S_render_node(cur, ev_type, &state, options);
  }
  result = (char *)cmark_strbuf_detach(&html);
  cmark_iter_free(iter);
  return result;
}
```

## Render State

```c
struct render_state {
  cmark_strbuf *html;    // Output buffer
  cmark_node *plain;     // Non-NULL when rendering image alt text (plain text mode)
};
```

The `plain` field is used for image alt text rendering. When entering an image node, `state->plain` is set to the image node. While `plain` is non-NULL, only text content is emitted (HTML tags are suppressed) — this ensures the `alt` attribute contains only plain text, not nested HTML. When the iterator exits the image node (`state->plain == node`), plain mode is cleared.

## HTML Escaping

```c
static void escape_html(cmark_strbuf *dest, const unsigned char *source,
                        bufsize_t length) {
  houdini_escape_html(dest, source, length, 0);
}
```

Characters `<`, `>`, `&`, `"` are converted to their HTML entity equivalents. The `0` argument means "not secure mode" (no additional escaping).

## Source Position Attributes

```c
static void S_render_sourcepos(cmark_node *node, cmark_strbuf *html, int options) {
  char buffer[BUFFER_SIZE];
  if (CMARK_OPT_SOURCEPOS & options) {
    snprintf(buffer, BUFFER_SIZE, " data-sourcepos=\"%d:%d-%d:%d\"",
             cmark_node_get_start_line(node), cmark_node_get_start_column(node),
             cmark_node_get_end_line(node), cmark_node_get_end_column(node));
    cmark_strbuf_puts(html, buffer);
  }
}
```

When `CMARK_OPT_SOURCEPOS` is set, all block-level elements receive a `data-sourcepos` attribute with format `"startline:startcol-endline:endcol"`.

## Newline Helper

```c
static inline void cr(cmark_strbuf *html) {
  if (html->size && html->ptr[html->size - 1] != '\n')
    cmark_strbuf_putc(html, '\n');
}
```

Ensures the output ends with a newline without adding redundant ones.

## Node Rendering Logic

The `S_render_node()` function handles each node type in a large switch statement. The `entering` boolean indicates whether this is an `CMARK_EVENT_ENTER` or `CMARK_EVENT_EXIT` event.

### Block Nodes

#### Document
No output — the document node is purely structural.

#### Block Quote
```
ENTER: \n<blockquote[sourcepos]>\n
EXIT:  \n</blockquote>\n
```

#### List
```
ENTER (bullet):  \n<ul[sourcepos]>\n
ENTER (ordered): \n<ol[sourcepos]>\n  (or <ol start="N"> if start > 1)
EXIT:            </ul>\n  or  </ol>\n
```

#### Item
```
ENTER: \n<li[sourcepos]>
EXIT:  </li>\n
```

#### Heading
```
ENTER: \n<hN[sourcepos]>   (where N = heading level)
EXIT:  </hN>\n
```

The heading level is injected into character arrays:
```c
char start_heading[] = "<h0";
start_heading[2] = (char)('0' + node->as.heading.level);
```

#### Code Block
Always a leaf node (single event). Output:
```html
<pre[sourcepos]><code>ESCAPED CONTENT</code></pre>\n
```

If the code block has an info string, a `class` attribute is added:
```html
<pre[sourcepos]><code class="language-INFO">ESCAPED CONTENT</code></pre>\n
```

The `"language-"` prefix is only added if the info string doesn't already start with `"language-"`.

#### HTML Block
When `CMARK_OPT_UNSAFE` is set, raw HTML is output verbatim. Otherwise, it's replaced with:
```html
<!-- raw HTML omitted -->
```

#### Thematic Break
```html
<hr[sourcepos] />\n
```

#### Paragraph
The paragraph respects tight list context. The renderer checks if the paragraph's grandparent is a list with `tight = true`:

```c
parent = cmark_node_parent(node);
grandparent = cmark_node_parent(parent);
if (grandparent != NULL && grandparent->type == CMARK_NODE_LIST) {
  tight = grandparent->as.list.tight;
} else {
  tight = false;
}
```

In tight lists, the `<p>` tags are suppressed — content flows directly without wrapping.

#### Custom Block
On enter, outputs the `on_enter` text literally. On exit, outputs `on_exit`.

### Inline Nodes

#### Text
```c
escape_html(html, node->data, node->len);
```

All text content is HTML-escaped.

#### Line Break
```html
<br />\n
```

#### Soft Break
Behavior depends on options:
- `CMARK_OPT_HARDBREAKS`: `<br />\n`
- `CMARK_OPT_NOBREAKS`: single space
- Default: `\n`

#### Code (inline)
```html
<code>ESCAPED CONTENT</code>
```

#### HTML Inline
Same as HTML block: verbatim with `CMARK_OPT_UNSAFE`, otherwise `<!-- raw HTML omitted -->`.

#### Emphasis
```
ENTER: <em>
EXIT:  </em>
```

#### Strong
```
ENTER: <strong>
EXIT:  </strong>
```

#### Link
```
ENTER: <a href="ESCAPED_URL"[ title="ESCAPED_TITLE"]>
EXIT:  </a>
```

URL safety: If `CMARK_OPT_UNSAFE` is NOT set, the URL is checked against `_scan_dangerous_url()`. Dangerous URLs (`javascript:`, `vbscript:`, `file:`, certain `data:` schemes) produce an empty `href`.

URL escaping uses `houdini_escape_href()` which percent-encodes special characters. Title escaping uses `escape_html()`.

#### Image
```
ENTER: <img src="ESCAPED_URL" alt="
  (enters plain text mode — state->plain = node)
EXIT:  "[ title="ESCAPED_TITLE"] />
```

During plain text mode (between enter and exit), only text content, code content, and HTML inline content are output (HTML-escaped), and breaks are rendered as spaces.

#### Custom Inline
On enter, outputs `on_enter` literally. On exit, outputs `on_exit`.

## URL Safety

Links and images check URL safety unless `CMARK_OPT_UNSAFE` is set:

```c
if (node->as.link.url && ((options & CMARK_OPT_UNSAFE) ||
                          !(_scan_dangerous_url(node->as.link.url)))) {
  houdini_escape_href(html, node->as.link.url,
                      (bufsize_t)strlen((char *)node->as.link.url));
}
```

The `_scan_dangerous_url()` scanner (from `scanners.c`) matches schemes: `javascript:`, `vbscript:`, `file:`, and `data:` (except for safe image MIME types: `image/png`, `image/gif`, `image/jpeg`, `image/webp`).

## Differences from Framework Renderers

The HTML renderer differs from the render-framework-based renderers in several ways:

1. **No line wrapping**: HTML output has no configurable width or word-wrap logic.
2. **No prefix management**: Block quotes and lists don't use prefix strings for indentation — they use HTML tags.
3. **Direct buffer writes**: All output goes directly to a `cmark_strbuf`, with no escaping dispatch function.
4. **No `width` parameter**: `cmark_render_html()` takes only `root` and `options`.

## Cross-References

- [html.c](../../cmark/src/html.c) — Full implementation
- [render-framework.md](render-framework.md) — The alternative render architecture used by other renderers
- [iterator-system.md](iterator-system.md) — How the AST is traversed
- [scanner-system.md](scanner-system.md) — `_scan_dangerous_url()` for URL safety
- [public-api.md](public-api.md) — `cmark_render_html()` API documentation
