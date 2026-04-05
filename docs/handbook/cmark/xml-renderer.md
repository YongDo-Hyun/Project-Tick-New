# cmark — XML Renderer

## Overview

The XML renderer (`xml.c`) produces an XML representation of the AST. Like the HTML renderer, it writes directly to a `cmark_strbuf` buffer rather than using the generic render framework. The output conforms to the CommonMark DTD.

## Entry Point

```c
char *cmark_render_xml(cmark_node *root, int options);
```

Returns a complete XML document string. The caller must free the result.

### Implementation

```c
char *cmark_render_xml(cmark_node *root, int options) {
  char *result;
  cmark_strbuf xml = CMARK_BUF_INIT(root->mem);
  cmark_event_type ev_type;
  cmark_node *cur;
  struct render_state state = {&xml, 0};
  cmark_iter *iter = cmark_iter_new(root);

  cmark_strbuf_puts(&xml,
                     "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n");

  // optionally: <?xml-model href="CommonMark.rnc" ...?>
  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    S_render_node(cur, ev_type, &state, options);
  }
  result = (char *)cmark_strbuf_detach(&xml);
  cmark_iter_free(iter);
  return result;
}
```

## Render State

```c
struct render_state {
  cmark_strbuf *xml;    // Output buffer
  int indent;           // Current indentation level (number of spaces)
};
```

The `indent` state tracks nesting depth, incremented by 2 for each container node entered.

## XML Escaping

```c
static CMARK_INLINE void escape_xml(cmark_strbuf *dest, const unsigned char *source,
                                    bufsize_t length) {
  houdini_escape_html0(dest, source, length, 0);
}
```

Escapes `<`, `>`, `&`, and `"` to their XML entity equivalents.

## Indentation

```c
static void indent(struct render_state *state) {
  int i;
  for (i = 0; i < state->indent; i++) {
    cmark_strbuf_putc(state->xml, ' ');
  }
}
```

Each level of nesting adds 2 spaces of indentation.

## Source Position Attributes

```c
static void S_render_sourcepos(cmark_node *node, cmark_strbuf *xml, int options) {
  char buffer[BUFFER_SIZE];
  if (CMARK_OPT_SOURCEPOS & options) {
    snprintf(buffer, BUFFER_SIZE, " sourcepos=\"%d:%d-%d:%d\"",
             cmark_node_get_start_line(node), cmark_node_get_start_column(node),
             cmark_node_get_end_line(node), cmark_node_get_end_column(node));
    cmark_strbuf_puts(xml, buffer);
  }
}
```

When `CMARK_OPT_SOURCEPOS` is active, XML elements receive `sourcepos="line:col-line:col"` attributes.

## Node Type Name Table

```c
static const char *S_type_string(cmark_node *node) {
  if (node->extension && node->extension->xml_tag_name_func) {
    return node->extension->xml_tag_name_func(node->extension, node);
  }
  switch (node->type) {
  case CMARK_NODE_DOCUMENT:       return "document";
  case CMARK_NODE_BLOCK_QUOTE:    return "block_quote";
  case CMARK_NODE_LIST:           return "list";
  case CMARK_NODE_ITEM:           return "item";
  case CMARK_NODE_CODE_BLOCK:     return "code_block";
  case CMARK_NODE_HTML_BLOCK:     return "html_block";
  case CMARK_NODE_CUSTOM_BLOCK:   return "custom_block";
  case CMARK_NODE_PARAGRAPH:      return "paragraph";
  case CMARK_NODE_HEADING:        return "heading";
  case CMARK_NODE_THEMATIC_BREAK: return "thematic_break";
  case CMARK_NODE_TEXT:           return "text";
  case CMARK_NODE_SOFTBREAK:     return "softbreak";
  case CMARK_NODE_LINEBREAK:     return "linebreak";
  case CMARK_NODE_CODE:          return "code";
  case CMARK_NODE_HTML_INLINE:   return "html_inline";
  case CMARK_NODE_CUSTOM_INLINE: return "custom_inline";
  case CMARK_NODE_EMPH:          return "emph";
  case CMARK_NODE_STRONG:        return "strong";
  case CMARK_NODE_LINK:          return "link";
  case CMARK_NODE_IMAGE:         return "image";
  case CMARK_NODE_NONE:          return "NONE";
  }
  return "<unknown>";
}
```

Each node type has a fixed XML tag name. Extensions can override this via `xml_tag_name_func`.

## Node Rendering Logic

### Leaf Nodes vs Container Nodes

The XML renderer distinguishes between leaf (literal) nodes and container nodes:

**Leaf nodes** (single event — `CMARK_EVENT_ENTER` only):
- `CMARK_NODE_CODE_BLOCK`, `CMARK_NODE_HTML_BLOCK`, `CMARK_NODE_THEMATIC_BREAK`
- `CMARK_NODE_TEXT`, `CMARK_NODE_SOFTBREAK`, `CMARK_NODE_LINEBREAK`
- `CMARK_NODE_CODE`, `CMARK_NODE_HTML_INLINE`

**Container nodes** (paired enter/exit events):
- `CMARK_NODE_DOCUMENT`, `CMARK_NODE_BLOCK_QUOTE`, `CMARK_NODE_LIST`, `CMARK_NODE_ITEM`
- `CMARK_NODE_PARAGRAPH`, `CMARK_NODE_HEADING`
- `CMARK_NODE_EMPH`, `CMARK_NODE_STRONG`, `CMARK_NODE_LINK`, `CMARK_NODE_IMAGE`
- `CMARK_NODE_CUSTOM_BLOCK`, `CMARK_NODE_CUSTOM_INLINE`

### Leaf Node Rendering

Literal nodes that contain text are rendered as:
```xml
  <tag_name>ESCAPED TEXT</tag_name>
```

For example, a text node with content "Hello & goodbye" becomes:
```xml
  <text>Hello &amp; goodbye</text>
```

Nodes without text content (thematic_break, softbreak, linebreak) are rendered as self-closing:
```xml
  <thematic_break />
```

### Container Node Rendering (Enter)

On enter, the renderer outputs:
```xml
  <tag_name[sourcepos][ type-specific attributes]>
```

And increments the indent level by 2.

#### Type-Specific Attributes on Enter

**List attributes:**
```c
cmark_strbuf_printf(xml, " type=\"%s\" tight=\"%s\"",
                    cmark_node_get_list_type(node) == CMARK_BULLET_LIST
                        ? "bullet" : "ordered",
                    cmark_node_get_list_tight(node) ? "true" : "false");
// For ordered lists only:
int start = cmark_node_get_list_start(node);
if (start != 1) {
  snprintf(buffer, BUFFER_SIZE, " start=\"%d\"", start);
}
cmark_strbuf_printf(xml, " delimiter=\"%s\"",
                    cmark_node_get_list_delim(node) == CMARK_PAREN_DELIM
                        ? "paren" : "period");
```

**Heading attributes:**
```c
snprintf(buffer, BUFFER_SIZE, " level=\"%d\"", node->as.heading.level);
```

**Code block attributes:**
```c
if (node->as.code.info) {
  cmark_strbuf_puts(xml, " info=\"");
  escape_xml(xml, node->as.code.info, (bufsize_t)strlen((char *)node->as.code.info));
  cmark_strbuf_putc(xml, '"');
}
```

**Link/Image attributes:**
```c
cmark_strbuf_puts(xml, " destination=\"");
escape_xml(xml, node->as.link.url, (bufsize_t)strlen((char *)node->as.link.url));
cmark_strbuf_putc(xml, '"');
cmark_strbuf_puts(xml, " title=\"");
escape_xml(xml, node->as.link.title, (bufsize_t)strlen((char *)node->as.link.title));
cmark_strbuf_putc(xml, '"');
```

**Custom block/inline attributes:**
```c
cmark_strbuf_puts(xml, " on_enter=\"");
escape_xml(xml, node->as.custom.on_enter, ...);
cmark_strbuf_puts(xml, "\" on_exit=\"");
escape_xml(xml, node->as.custom.on_exit, ...);
```

### Container Node Rendering (Exit)

On exit, the indent level is decremented by 2, and the closing tag is output:
```xml
  </tag_name>
```

### Extension Support

Extensions can add additional XML attributes via:
```c
if (node->extension && node->extension->xml_attr_func) {
  node->extension->xml_attr_func(node->extension, node, xml);
}
```

## Example Output

Given this Markdown:

```markdown
# Hello

A paragraph with *emphasis* and a [link](http://example.com "title").
```

The XML output (with `CMARK_OPT_SOURCEPOS`):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE document SYSTEM "CommonMark.dtd">
<document sourcepos="1:1-3:65" xmlns="http://commonmark.org/xml/1.0">
  <heading sourcepos="1:1-1:7" level="1">
    <text>Hello</text>
  </heading>
  <paragraph sourcepos="3:1-3:65">
    <text>A paragraph with </text>
    <emph>
      <text>emphasis</text>
    </emph>
    <text> and a </text>
    <link destination="http://example.com" title="title">
      <text>link</text>
    </link>
    <text>.</text>
  </paragraph>
</document>
```

## CommonMark DTD

The output references `CommonMark.dtd`, the DTD that defines:
- Document element as the root
- All CommonMark block and inline element types
- Required attributes for lists, headings, links, images, and code blocks
- Entity definitions for the markup model

## Differences from HTML Renderer

1. **Full AST preservation**: XML represents the complete AST structure, including node types that HTML merges or loses (e.g., softbreak, custom blocks/inlines).
2. **Indentation tracking**: XML output is pretty-printed with nesting-based indentation.
3. **No tight list logic**: The `tight` attribute is stored as metadata, but does not affect paragraph rendering — paragraphs always appear as `<paragraph>` elements.
4. **No URL safety**: URLs are output as-is (escaped for XML), no `_scan_dangerous_url()` check.
5. **No plain text mode**: Image children are rendered structurally, not flattened to alt text.

## Cross-References

- [xml.c](../../cmark/src/xml.c) — Full implementation
- [html-renderer.md](html-renderer.md) — HTML renderer comparison
- [iterator-system.md](iterator-system.md) — Traversal mechanism used
- [public-api.md](public-api.md) — `cmark_render_xml()` API docs
