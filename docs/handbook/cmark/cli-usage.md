# cmark — CLI Usage

## Overview

The `cmark` command-line tool (`main.c`) reads CommonMark input from files or stdin and renders it to one of five output formats. It serves as both a reference implementation and a practical conversion tool.

## Entry Point

```c
int main(int argc, char *argv[]);
```

## Output Formats

```c
typedef enum {
  FORMAT_NONE,
  FORMAT_HTML,
  FORMAT_XML,
  FORMAT_MAN,
  FORMAT_COMMONMARK,
  FORMAT_LATEX,
} writer_format;
```

Default: `FORMAT_HTML`.

## Command-Line Options

| Option | Long Form | Description |
|--------|-----------|-------------|
| `-t FORMAT` | `--to FORMAT` | Output format: `html`, `xml`, `man`, `commonmark`, `latex` |
| | `--width N` | Wrapping width (0 = no wrapping; default 0). Only affects `commonmark`, `man`, `latex` |
| | `--sourcepos` | Include source position information |
| | `--hardbreaks` | Render soft breaks as hard breaks |
| | `--nobreaks` | Render soft breaks as spaces |
| | `--unsafe` | Allow raw HTML and dangerous URLs |
| | `--smart` | Enable smart punctuation (curly quotes, em/en dashes, ellipses) |
| | `--validate-utf8` | Validate and clean UTF-8 input |
| `-h` | `--help` | Print usage information |
| | `--version` | Print version string |

## Option Parsing

```c
for (i = 1; i < argc; i++) {
  if (strcmp(argv[i], "--version") == 0) {
    printf("cmark %s", cmark_version_string());
    printf(" - CommonMark converter\n(C) 2014-2016 John MacFarlane\n");
    exit(0);
  } else if (strcmp(argv[i], "--sourcepos") == 0) {
    options |= CMARK_OPT_SOURCEPOS;
  } else if (strcmp(argv[i], "--hardbreaks") == 0) {
    options |= CMARK_OPT_HARDBREAKS;
  } else if (strcmp(argv[i], "--nobreaks") == 0) {
    options |= CMARK_OPT_NOBREAKS;
  } else if (strcmp(argv[i], "--smart") == 0) {
    options |= CMARK_OPT_SMART;
  } else if (strcmp(argv[i], "--unsafe") == 0) {
    options |= CMARK_OPT_UNSAFE;
  } else if (strcmp(argv[i], "--validate-utf8") == 0) {
    options |= CMARK_OPT_VALIDATE_UTF8;
  } else if ((strcmp(argv[i], "--to") == 0 || strcmp(argv[i], "-t") == 0) &&
             i + 1 < argc) {
    i++;
    if (strcmp(argv[i], "man") == 0)           writer = FORMAT_MAN;
    else if (strcmp(argv[i], "html") == 0)     writer = FORMAT_HTML;
    else if (strcmp(argv[i], "xml") == 0)      writer = FORMAT_XML;
    else if (strcmp(argv[i], "commonmark") == 0) writer = FORMAT_COMMONMARK;
    else if (strcmp(argv[i], "latex") == 0)    writer = FORMAT_LATEX;
    else {
      fprintf(stderr, "Unknown format %s\n", argv[i]);
      exit(1);
    }
  } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
    i++;
    width = atoi(argv[i]);
  } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
    print_usage();
    exit(0);
  } else if (*argv[i] == '-') {
    print_usage();
    exit(1);
  } else {
    // Treat as filename
    files[numfps++] = i;
  }
}
```

## Input Handling

### File Input

```c
for (i = 0; i < numfps; i++) {
  fp = fopen(argv[files[i]], "rb");
  if (fp == NULL) {
    fprintf(stderr, "Error opening file %s: %s\n", argv[files[i]], strerror(errno));
    exit(1);
  }
  // Read in chunks and feed to parser
  while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
    cmark_parser_feed(parser, buffer, bytes);
    if (bytes < sizeof(buffer)) break;
  }
  fclose(fp);
}
```

Files are opened in binary mode (`"rb"`) and read in chunks of `BUFFER_SIZE` (4096 bytes). Each chunk is fed to the streaming parser via `cmark_parser_feed()`.

### Stdin Input

```c
if (numfps == 0) {
  // Read from stdin
  while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
    cmark_parser_feed(parser, buffer, bytes);
    if (bytes < sizeof(buffer)) break;
  }
}
```

When no files are specified, input is read from stdin.

### Windows Binary Mode

```c
#if defined(_WIN32) && !defined(__CYGWIN__)
_setmode(_fileno(stdin), _O_BINARY);
_setmode(_fileno(stdout), _O_BINARY);
#endif
```

On Windows, stdin and stdout are set to binary mode to prevent CR/LF translation.

## Rendering

```c
document = cmark_parser_finish(parser);
cmark_parser_free(parser);

// Render based on format
result = print_document(document, writer, width, options);
```

### `print_document()`

```c
static void print_document(cmark_node *document, writer_format writer,
                           int width, int options) {
  char *result;
  switch (writer) {
  case FORMAT_HTML:
    result = cmark_render_html(document, options);
    break;
  case FORMAT_XML:
    result = cmark_render_xml(document, options);
    break;
  case FORMAT_MAN:
    result = cmark_render_man(document, options, width);
    break;
  case FORMAT_COMMONMARK:
    result = cmark_render_commonmark(document, options, width);
    break;
  case FORMAT_LATEX:
    result = cmark_render_latex(document, options, width);
    break;
  default:
    fprintf(stderr, "Unknown format %d\n", writer);
    exit(1);
  }
  printf("%s", result);
  document->mem->free(result);
}
```

The rendered result is written to stdout and then freed.

### Cleanup

```c
cmark_node_free(document);
```

The AST is freed after rendering.

## OpenBSD Security

```c
#ifdef __OpenBSD__
  if (pledge("stdio rpath", NULL) != 0) {
    perror("pledge");
    return 1;
  }
#endif
```

On OpenBSD, the program restricts itself to `stdio` and `rpath` (read-only file access) via `pledge()`. This prevents the cmark binary from performing any operations beyond reading files and writing to stdout/stderr.

## Usage Examples

```bash
# Convert Markdown to HTML
cmark input.md

# Convert with smart punctuation
cmark --smart input.md

# Convert to man page with 72-column wrapping
cmark -t man --width 72 input.md

# Convert to LaTeX
cmark -t latex input.md

# Round-trip through CommonMark
cmark -t commonmark input.md

# Include source positions in output
cmark --sourcepos input.md

# Allow raw HTML passthrough
cmark --unsafe input.md

# Read from stdin
echo "# Hello" | cmark

# Validate UTF-8 input
cmark --validate-utf8 input.md

# Print version
cmark --version
```

## Exit Codes

- `0` — Success
- `1` — Error (unknown option, file open failure, unknown format)

## Cross-References

- [main.c](../../cmark/src/main.c) — Full implementation
- [public-api.md](public-api.md) — The C API functions called by main
- [html-renderer.md](html-renderer.md) — `cmark_render_html()`
- [xml-renderer.md](xml-renderer.md) — `cmark_render_xml()`
- [latex-renderer.md](latex-renderer.md) — `cmark_render_latex()`
- [man-renderer.md](man-renderer.md) — `cmark_render_man()`
- [commonmark-renderer.md](commonmark-renderer.md) — `cmark_render_commonmark()`
