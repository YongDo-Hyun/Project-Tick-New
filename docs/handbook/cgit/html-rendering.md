# cgit — HTML Rendering Engine

## Overview

cgit generates all HTML output through a set of low-level rendering functions
defined in `html.c` and `html.h`.  These functions handle entity escaping,
URL encoding, and formatted output.  Higher-level page structure is built by
`ui-shared.c`.

Source files: `html.c`, `html.h`, `ui-shared.c`, `ui-shared.h`.

## Output Model

All output functions write directly to `stdout` via `write(2)`.  There is no
internal buffering beyond the standard I/O buffer.  This design works because
cgit runs as a CGI process — each request is a separate process with its own
stdout connected to the web server.

## Core Output Functions

### Raw Output

```c
void html_raw(const char *data, size_t size);
```

Writes raw bytes to stdout without any escaping.  Used for binary content
and pre-escaped strings.

### Escaped Text Output

```c
void html(const char *txt);
```

Writes a string with HTML entity escaping:
- `<` → `&lt;`
- `>` → `&gt;`
- `&` → `&amp;`

```c
void html_txt(const char *txt);
```

Same as `html()` but also escapes:
- `"` → `&quot;`
- `'` → `&#x27;`

Used for text content that appears inside HTML tags.

```c
void html_ntxt(const char *txt, int len);
```

Length-limited version of `html_txt()`.  Writes at most `len` characters,
appending `...` if truncated.

### Attribute Escaping

```c
void html_attr(const char *txt);
```

Escapes text for use in HTML attribute values.  Escapes the same characters
as `html_txt()`.

## URL Encoding

### URL Escape Table

`html.c` defines a 256-entry escape table for URL encoding:

```c
static const char *url_escape_table[256] = {
    "%00", "%01", "%02", ...,
    [' ']  = "+",
    ['!']  = NULL,    /* pass through */
    ['"']  = "%22",
    ['#']  = "%23",
    ['%']  = "%25",
    ['&']  = "%26",
    ['+']  = "%2B",
    ['?']  = "%3F",
    /* letters, digits, '-', '_', '.', '~' pass through (NULL) */
    ...
};
```

Characters with a `NULL` entry pass through unmodified.  All others are
replaced with their percent-encoded representations.

### URL Path Encoding

```c
void html_url_path(const char *txt);
```

Encodes a URL path component.  Uses `url_escape_table` but preserves `/`
characters (they are structural in paths).

### URL Argument Encoding

```c
void html_url_arg(const char *txt);
```

Encodes a URL query parameter value.  Uses `url_escape_table` including
encoding `/` characters.

## Formatted Output

### `fmt()` — Ring Buffer Formatter

```c
const char *fmt(const char *format, ...);
```

A `printf`-style formatter that returns a pointer to an internal static
buffer.  Uses a ring of 8 buffers (each 8 KB) to allow multiple `fmt()`
calls in a single expression:

```c
#define FMT_BUFS 8
#define FMT_SIZE 8192

static char bufs[FMT_BUFS][FMT_SIZE];
static int bufidx;

const char *fmt(const char *format, ...)
{
    bufidx = (bufidx + 1) % FMT_BUFS;
    va_list args;
    va_start(args, format);
    vsnprintf(bufs[bufidx], FMT_SIZE, format, args);
    va_end(args);
    return bufs[bufidx];
}
```

This is used extensively throughout cgit for constructing strings without
explicit memory management.  The ring buffer avoids use-after-free for up to
8 nested calls.

### `fmtalloc()` — Heap Formatter

```c
char *fmtalloc(const char *format, ...);
```

Like `fmt()` but allocates a new heap buffer with `xstrfmt()`.  Used when
the result must outlive the ring buffer cycle.

### `htmlf()` — Formatted HTML

```c
void htmlf(const char *format, ...);
```

`printf`-style output directly to stdout.  Does NOT perform HTML escaping —
the caller must ensure the format string and arguments are safe.

## Form Helpers

### Hidden Fields

```c
void html_hidden(const char *name, const char *value);
```

Generates a hidden form field:

```html
<input type='hidden' name='name' value='value' />
```

Values are attribute-escaped.

### Option Elements

```c
void html_option(const char *value, const char *text, const char *selected_value);
```

Generates an `<option>` element, marking it as selected if `value` matches
`selected_value`:

```html
<option value='value' selected='selected'>text</option>
```

### Checkbox Input

```c
void html_checkbox(const char *name, int value);
```

Generates a checkbox input.

### Text Input

```c
void html_txt_input(const char *name, const char *value, int size);
```

Generates a text input field.

## Link Generation

```c
void html_link_open(const char *url, const char *title, const char *class);
void html_link_close(void);
```

Generate `<a>` tags with optional title and class attributes.  URL is
path-escaped.

## File Inclusion

```c
void html_include(const char *filename);
```

Reads a file from disk and writes its contents to stdout without escaping.
Used for header/footer file inclusion configured via the `header` and
`footer` directives.

## Page Structure (`ui-shared.c`)

### HTTP Headers

```c
void cgit_print_http_headers(void);
```

Emits HTTP response headers based on `ctx.page`:

```
Status: 200 OK
Content-Type: text/html; charset=utf-8
Last-Modified: Thu, 01 Jan 2024 00:00:00 GMT
Expires: Thu, 01 Jan 2024 01:00:00 GMT
ETag: "abc123"
```

Fields are only emitted when the corresponding `ctx.page` fields are set.

### HTML Document Head

```c
void cgit_print_docstart(void);
```

Emits the HTML5 doctype, `<html>`, and `<head>` section:

```html
<!DOCTYPE html>
<html lang='en'>
<head>
  <title>repo - page</title>
  <meta name='generator' content='cgit v0.0.5-1-Project-Tick'/>
  <meta name='robots' content='index, nofollow'/>
  <link rel='stylesheet' href='/cgit/cgit.css'/>
  <link rel='icon' href='/favicon.ico'/>
</head>
```

### Page Header

```c
void cgit_print_pageheader(void);
```

Renders the page header with logo, navigation tabs, and search form.
Navigation tabs are context-sensitive — repository pages show
summary/refs/log/tree/commit/diff/stats/etc.

### Page Footer

```c
void cgit_print_docend(void);
```

Closes the HTML document with footer content and closing tags.

### Full Page Layout

```c
void cgit_print_layout_start(void);
void cgit_print_layout_end(void);
```

These wrap the page content, calling `cgit_print_http_headers()`,
`cgit_print_docstart()`, `cgit_print_pageheader()`, etc.  Commands with
`want_layout=1` have their output wrapped in this skeleton.

## Repository Navigation

```c
void cgit_print_repoheader(void);
```

For each page within a repository, renders:
- Repository name and description
- Navigation tabs: summary, refs, log, tree, commit, diff, stats
- Clone URLs
- Badges

## Link Functions

`ui-shared.c` provides numerous helper functions for generating
context-aware links:

```c
void cgit_summary_link(const char *name, const char *title,
                       const char *class, const char *head);
void cgit_tag_link(const char *name, const char *title,
                   const char *class, const char *tag);
void cgit_tree_link(const char *name, const char *title,
                    const char *class, const char *head,
                    const char *rev, const char *path);
void cgit_log_link(const char *name, const char *title,
                   const char *class, const char *head,
                   const char *rev, const char *path,
                   int ofs, const char *grep, const char *pattern,
                   int showmsg, int follow);
void cgit_commit_link(const char *name, const char *title,
                      const char *class, const char *head,
                      const char *rev, const char *path);
void cgit_patch_link(const char *name, const char *title,
                     const char *class, const char *head,
                     const char *rev, const char *path);
void cgit_refs_link(const char *name, const char *title,
                    const char *class, const char *head,
                    const char *rev, const char *path);
void cgit_diff_link(const char *name, const char *title,
                    const char *class, const char *head,
                    const char *new_rev, const char *old_rev,
                    const char *path, int toggle_hierarchical_threading);
void cgit_stats_link(const char *name, const char *title,
                     const char *class, const char *head,
                     const char *path);
void cgit_plain_link(const char *name, const char *title,
                     const char *class, const char *head,
                     const char *rev, const char *path);
void cgit_blame_link(const char *name, const char *title,
                     const char *class, const char *head,
                     const char *rev, const char *path);
void cgit_object_link(struct object *obj);
void cgit_submodule_link(const char *name, const char *path,
                         const char *commit);
```

Each function builds a complete `<a>` tag with the appropriate URL, including
all required query parameters for the target page.

## Diff Output Helpers

```c
void cgit_print_diff_hunk_header(int oldofs, int oldcnt,
                                  int newofs, int newcnt, const char *func);
void cgit_print_diff_line_prefix(int type);
```

These render diff hunks with proper CSS classes for syntax coloring (`.add`,
`.del`, `.hunk`).

## Error Pages

```c
void cgit_print_error(const char *msg);
void cgit_print_error_page(int code, const char *msg, const char *fmt, ...);
```

`cgit_print_error_page()` sets the HTTP status code and wraps the error
message in a full page layout.

## Encoding

All text output assumes UTF-8.  The `Content-Type` header is always
`charset=utf-8`.  There is no character set conversion.
