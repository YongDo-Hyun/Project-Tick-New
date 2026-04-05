# cmark — LaTeX Renderer

## Overview

The LaTeX renderer (`latex.c`) converts a `cmark_node` AST into LaTeX source, suitable for compilation with `pdflatex`, `xelatex`, or `lualatex`. It uses the generic render framework from `render.c`, operating through a per-character output callback (`outc`) and a per-node render callback (`S_render_node`).

## Entry Point

```c
char *cmark_render_latex(cmark_node *root, int options, int width);
```

- `root` — AST root node
- `options` — Option flags (`CMARK_OPT_SOURCEPOS`, `CMARK_OPT_HARDBREAKS`, `CMARK_OPT_NOBREAKS`, `CMARK_OPT_UNSAFE`)
- `width` — Target line width for hard-wrapping; 0 disables wrapping

## Character Escaping (`outc`)

The `outc` function handles per-character output decisions. It is the most complex part of the LaTeX renderer, with different behavior for three escaping modes:

```c
static void outc(cmark_renderer *renderer, cmark_escaping escape,
                 int32_t c, unsigned char nextc);
```

### LITERAL Mode
Pass-through: all characters are output unchanged.

### NORMAL Mode
Extensive special-character handling:

| Character | LaTeX Output | Purpose |
|-----------|-------------|---------|
| `$` | `\$` | Math mode delimiter |
| `%` | `\%` | Comment character |
| `&` | `\&` | Table column separator |
| `_` | `\_` | Subscript operator |
| `#` | `\#` | Parameter reference |
| `^` | `\^{}` | Superscript operator |
| `{` | `\{` | Group open |
| `}` | `\}` | Group close |
| `~` | `\textasciitilde{}` | Non-breaking space |
| `[` | `{[}` | Optional argument bracket |
| `]` | `{]}` | Optional argument bracket |
| `\` | `\textbackslash{}` | Escape character |
| `|` | `\textbar{}` | Pipe |
| `'` | `\textquotesingle{}` | Straight single quote |
| `"` | `\textquotedbl{}` | Straight double quote |
| `` ` `` | `\textasciigrave{}` | Backtick |
| `\xA0` (NBSP) | `~` | LaTeX non-breaking space |
| `\x2014` (—) | `---` | Em dash |
| `\x2013` (–) | `--` | En dash |
| `\x2018` (') | `` ` `` | Left single quote |
| `\x2019` (') | `'` | Right single quote |
| `\x201C` (") | ` `` ` | Left double quote |
| `\x201D` (") | `''` | Right double quote |

### URL Mode
Only these characters are escaped:
- `$` → `\$`
- `%` → `\%`
- `&` → `\&`
- `_` → `\_`
- `#` → `\#`
- `{` → `\{`
- `}` → `\}`

All other characters pass through unchanged.

## Link Type Classification

The renderer classifies links into five categories:

```c
typedef enum {
  NO_LINK,
  URL_AUTOLINK,
  EMAIL_AUTOLINK,
  NORMAL_LINK,
  INTERNAL_LINK,
} link_type;
```

### `get_link_type()`

```c
static link_type get_link_type(cmark_node *node) {
  // 1. "mailto:" links where text matches url
  // 2. "http[s]:" links where text matches url (with or without protocol)
  // 3. Links starting with '#' → INTERNAL_LINK
  // 4. Everything else → NORMAL_LINK
}
```

Detection logic:
1. **URL_AUTOLINK**: The `url` starts with `http://` or `https://`, the link has exactly one text child, and that child's content matches the URL (or matches the URL minus the protocol prefix).
2. **EMAIL_AUTOLINK**: The `url` starts with `mailto:`, the link has exactly one text child, and that child's content matches the URL after `mailto:`.
3. **INTERNAL_LINK**: The `url` starts with `#`.
4. **NORMAL_LINK**: Everything else.

## Enumeration Level

For nested ordered lists, the renderer selects the appropriate LaTeX counter style:

```c
static int S_get_enumlevel(cmark_node *node) {
  int enumlevel = 0;
  cmark_node *tmp = node;
  while (tmp) {
    if (tmp->type == CMARK_NODE_LIST &&
        cmark_node_get_list_type(tmp) == CMARK_ORDERED_LIST) {
      enumlevel++;
    }
    tmp = tmp->parent;
  }
  return enumlevel;
}
```

This walks up the tree, counting ordered list ancestors. LaTeX ordered lists cycle through: `enumi` (arabic), `enumii` (alpha), `enumiii` (roman), `enumiv` (Alpha).

## Node Rendering (`S_render_node`)

### Block Nodes

#### Document
No output.

#### Block Quote
```
ENTER: \begin{quote}\n
EXIT:  \end{quote}\n
```

#### List
```
ENTER (bullet):  \begin{itemize}\n
ENTER (ordered): \begin{enumerate}\n
                 \def\labelenumI{COUNTER}\n  (if start != 1)
                 \setcounter{enumI}{START-1}\n
EXIT:            \end{itemize}\n or \end{enumerate}\n
```

The counter is formatted based on enumeration level:
- Level 1: `\arabic{enumi}.`
- Level 2: `\alph{enumii}.` (surrounded by `(`)
- Level 3: `\roman{enumiii}.`
- Level 4: `\Alph{enumiv}.`

Period delimiters use `.`, parenthesis delimiters use `)`.

#### Item
```
ENTER: \item{}   (empty braces prevent ligatures with following content)
EXIT:  \n
```

#### Heading
```
ENTER: \section{  or  \subsection{  or  \subsubsection{  or  \paragraph{  or  \subparagraph{
EXIT:  }\n
```

Mapping: level 1 → `\section`, level 2 → `\subsection`, level 3 → `\subsubsection`, level 4 → `\paragraph`, level 5 → `\subparagraph`.

#### Code Block
```latex
\begin{verbatim}
LITERAL CONTENT
\end{verbatim}
```

The content is output in `LITERAL` escape mode (no character escaping). Info strings are ignored.

#### HTML Block
```
ENTER: % raw HTML omitted\n  (as a LaTeX comment)
```

Raw HTML is always omitted in LaTeX output, regardless of `CMARK_OPT_UNSAFE`.

#### Thematic Break
```
\begin{center}\rule{0.5\linewidth}{\linethickness}\end{center}\n
```

#### Paragraph
Same tight-list check as the HTML renderer:
```c
parent = cmark_node_parent(node);
grandparent = cmark_node_parent(parent);
tight = (grandparent && grandparent->type == CMARK_NODE_LIST) ?
        grandparent->as.list.tight : false;
```
- Normal: newline before and after
- Tight: no leading/trailing blank lines

### Inline Nodes

#### Text
Output with NORMAL escaping.

#### Soft Break
Depends on options:
- `CMARK_OPT_HARDBREAKS`: `\\\\\n`
- `CMARK_OPT_NOBREAKS`: space
- Default: newline

#### Line Break
```
\\\\\n
```

#### Code (inline)
```
\texttt{ESCAPED CONTENT}
```

Special handling: Code content is output character-by-character with inline-code escaping. Special characters (`\`, `{`, `}`, `$`, `%`, `&`, `_`, `#`, `^`, `~`) are escaped.

#### Emphasis
```
ENTER: \emph{
EXIT:  }
```

#### Strong
```
ENTER: \textbf{
EXIT:  }
```

#### Link
Rendering depends on link type:

**NORMAL_LINK:**
```
ENTER: \href{URL}{
EXIT:  }
```

**URL_AUTOLINK:**
```
ENTER: \url{URL}
(children are skipped — no EXIT rendering needed)
```

**EMAIL_AUTOLINK:**
```
ENTER: \href{URL}{\nolinkurl{
EXIT:  }}
```

**INTERNAL_LINK:**
```
ENTER: (nothing — rendered as plain text)
EXIT:  (~\ref{LABEL})
```

Where `LABEL` is the URL with the leading `#` stripped.

**NO_LINK:**
No output.

#### Image
```
ENTER: \protect\includegraphics{URL}
```

Image children (alt text) are skipped. If `CMARK_OPT_UNSAFE` is not set and the URL matches `_scan_dangerous_url()`, the URL is omitted.

#### HTML Inline
```
% raw HTML omitted
```

Always omitted, regardless of `CMARK_OPT_UNSAFE`.

## Source Position Comments

When `CMARK_OPT_SOURCEPOS` is set, the renderer adds LaTeX comments before block elements:

```c
snprintf(buffer, BUFFER_SIZE, "%% %d:%d-%d:%d\n",
         cmark_node_get_start_line(node), cmark_node_get_start_column(node),
         cmark_node_get_end_line(node), cmark_node_get_end_column(node));
```

## Example Output

Markdown input:
```markdown
# Hello World

A paragraph with *emphasis* and **bold**.

- Item 1
- Item 2
```

LaTeX output:
```latex
\section{Hello World}

A paragraph with \emph{emphasis} and \textbf{bold}.

\begin{itemize}
\item{}Item 1

\item{}Item 2

\end{itemize}
```

## Cross-References

- [latex.c](../../cmark/src/latex.c) — Full implementation
- [render-framework.md](render-framework.md) — Generic render framework (`cmark_render()`, `cmark_renderer`)
- [public-api.md](public-api.md) — `cmark_render_latex()` API docs
- [html-renderer.md](html-renderer.md) — Contrast with direct buffer renderer
