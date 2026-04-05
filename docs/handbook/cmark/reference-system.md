# cmark — Reference System

## Overview

The reference system (`references.c`, `references.h`) manages link reference definitions — the `[label]: URL "title"` constructs in CommonMark. During block parsing, reference definitions are extracted and stored. During inline parsing, reference links (`[text][label]` and `[text]`) look up these stored definitions.

## Data Structures

### Reference Entry

```c
typedef struct cmark_reference {
  struct cmark_reference *next;   // Unused — leftover from old linked-list design
  unsigned char *url;
  unsigned char *title;
  unsigned char *label;
  unsigned int age;               // Insertion order (for stable sorting)
  unsigned int size;              // Length of the label string
} cmark_reference;
```

Each reference stores:
- `label` — The normalized reference label (case-folded, whitespace-collapsed)
- `url` — The destination URL
- `title` — Optional title string (may be NULL)
- `age` — Monotonically increasing counter for insertion order
- `size` — Byte length of the label

### Reference Map

```c
struct cmark_reference_map {
  cmark_mem *mem;
  cmark_reference **refs;    // Sorted array of reference pointers
  unsigned int size;         // Number of entries
  unsigned int ref_size;     // Cumulative size of all labels + URLs + titles
  unsigned int max_ref_size; // Maximum allowed ref_size (anti-DoS limit)
  cmark_reference *last;     // Most recently added reference
  unsigned int asize;        // Allocated capacity of refs array
};
```

The map uses a **sorted array with binary search** for lookup, not a hash table. This gives O(log n) lookup and O(n) insertion with shifting.

### Anti-DoS Limiting

The `ref_size` and `max_ref_size` fields prevent pathological inputs from causing excessive memory usage:

```c
unsigned int max_ref_size;  // Set to 100 * input length at parser init
unsigned int ref_size;      // Sum of all label + url + title lengths
```

When `ref_size` exceeds `max_ref_size`, new reference additions are silently rejected. This prevents quadratic memory blowup from inputs with many reference definitions.

## Label Normalization

```c
static unsigned char *normalize_reference(cmark_mem *mem,
                                          cmark_chunk *ref) {
  cmark_strbuf normalized = CMARK_BUF_INIT(mem);

  if (ref == NULL) return NULL;

  if (ref->len == 0) return NULL;

  cmark_utf8proc_case_fold(&normalized, ref->data, ref->len);
  cmark_strbuf_trim(&normalized);
  cmark_strbuf_normalize_whitespace(&normalized);

  return cmark_strbuf_detach(&normalized);
}
```

The normalization process (per CommonMark spec):
1. **Case fold** — Uses Unicode case folding (not simple lowercasing), via `cmark_utf8proc_case_fold()`
2. **Trim** — Remove leading and trailing whitespace
3. **Collapse whitespace** — Replace runs of whitespace with a single space

This means `[Foo Bar]`, `[FOO   BAR]`, and `[foo bar]` all normalize to the same label.

## Reference Creation

```c
static void cmark_reference_create(cmark_reference_map *map,
                                   cmark_chunk *label,
                                   cmark_chunk *url,
                                   cmark_chunk *title) {
  cmark_reference *ref;
  unsigned char *reflabel = normalize_reference(map->mem, label);

  if (reflabel == NULL) return;

  // Anti-DoS: check cumulative size limit
  if (map->ref_size > map->max_ref_size) {
    map->mem->free(reflabel);
    return;
  }

  ref = (cmark_reference *)map->mem->calloc(1, sizeof(*ref));
  ref->label = reflabel;
  ref->url = cmark_clean_url(map->mem, url);
  ref->title = cmark_clean_title(map->mem, title);
  ref->age = map->size;
  ref->size = (unsigned int)strlen((char *)reflabel);

  // Track cumulative size
  map->ref_size += ref->size;
  if (ref->url) map->ref_size += (unsigned int)strlen((char *)ref->url);
  if (ref->title) map->ref_size += (unsigned int)strlen((char *)ref->title);

  // Add to array
  if (map->size >= map->asize) {
    // Grow array (double capacity)
    map->asize = map->asize ? 2 * map->asize : 8;
    map->refs = (cmark_reference **)map->mem->realloc(
        map->refs, map->asize * sizeof(cmark_reference *));
  }
  map->refs[map->size] = ref;
  map->size++;
  map->last = ref;
}
```

References are appended to the array in insertion order. The array is NOT kept sorted during insertion — it's sorted once at lookup time (lazily).

## Reference Lookup

```c
cmark_reference *cmark_reference_lookup(cmark_reference_map *map,
                                        cmark_chunk *label) {
  if (label->len < 1 || label->len > MAX_LINK_LABEL_LENGTH) return NULL;
  if (map == NULL || map->size == 0) return NULL;

  unsigned char *norm = normalize_reference(map->mem, label);
  if (norm == NULL) return NULL;

  // Sort on first lookup
  if (!map->sorted) {
    qsort(map->refs, map->size, sizeof(cmark_reference *), refcmp);
    // Remove duplicates (keep first occurrence)
    // ...
    map->sorted = true;
  }

  // Binary search
  cmark_reference **found = (cmark_reference **)bsearch(
      &norm, map->refs, map->size, sizeof(cmark_reference *), refcmp);

  map->mem->free(norm);
  return found ? *found : NULL;
}
```

### Lazy Sorting

The reference map is NOT sorted during insertion. On the first call to `cmark_reference_lookup()`, the array is sorted using `qsort()` with a comparison function:

```c
static int refcmp(const void *a, const void *b) {
  const cmark_reference *refa = *(const cmark_reference **)a;
  const cmark_reference *refb = *(const cmark_reference **)b;
  int cmp = strcmp((char *)refa->label, (char *)refb->label);
  if (cmp != 0) return cmp;
  // Tie-break by age (earlier wins)
  if (refa->age < refb->age) return -1;
  if (refa->age > refb->age) return 1;
  return 0;
}
```

When labels collide (same normalized label), the first definition wins (lowest `age`).

After sorting, duplicates are removed — entries with the same label as the preceding entry are freed:
```c
unsigned int write = 0;
for (unsigned int read = 0; read < map->size; read++) {
  if (write > 0 &&
      strcmp((char *)map->refs[write-1]->label,
             (char *)map->refs[read]->label) == 0) {
    // Duplicate — free it
    cmark_reference_free(map->mem, map->refs[read]);
  } else {
    map->refs[write++] = map->refs[read];
  }
}
map->size = write;
```

### Binary Search

After sorting and deduplication, lookups use standard `bsearch()`, giving O(log n) lookup time.

## URL and Title Cleaning

When creating references, URLs and titles are cleaned:

### `cmark_clean_url()`
```c
unsigned char *cmark_clean_url(cmark_mem *mem, cmark_chunk *url);
```
- Removes surrounding `<` and `>` if present (angle-bracket URLs)
- Unescapes backslash escapes
- Decodes entity references
- Percent-encodes non-URL-safe characters via `houdini_escape_href()`

### `cmark_clean_title()`
```c
unsigned char *cmark_clean_title(cmark_mem *mem, cmark_chunk *title);
```
- Strips the first and last character (the delimiter: `"`, `'`, or `(`)
- Unescapes backslash escapes
- Decodes entity references

## Integration with Parser

### Extraction during Block Parsing

Reference definitions are extracted when paragraphs are finalized:

```c
// In blocks.c, during paragraph finalization:
while (cmark_parse_reference_inline(parser->mem, &node_content,
                                    parser->refmap)) {
  // Keep parsing references from the start of the paragraph
}
```

### `cmark_parse_reference_inline()`

```c
int cmark_parse_reference_inline(cmark_mem *mem, cmark_strbuf *input,
                                 cmark_reference_map *refmap) {
  // Parse: [label]: destination "title"
  // Returns 1 if a reference was found and consumed, 0 otherwise
  subject subj;
  // ... initialize subject on the input buffer
  // Parse label
  cmark_chunk lab = cmark_chunk_literal("");
  cmark_chunk url = cmark_chunk_literal("");
  cmark_chunk title = cmark_chunk_literal("");

  if (!link_label(&subj, &lab) || lab.len == 0) return 0;
  if (peek_char(&subj) != ':') return 0;
  advance(&subj);
  spnl(&subj);  // skip spaces and up to one newline
  if (!manual_scan_link_url(&subj, &url)) return 0;
  // ... parse optional title
  // ... validate: rest of line must be blank
  cmark_reference_create(refmap, &lab, &url, &title);
  // Remove consumed bytes from input
  return 1;
}
```

The parser repeatedly calls this function on paragraph content. Each successful parse removes the reference definition from the paragraph. If the entire paragraph consists of reference definitions, the paragraph node is removed from the AST.

### Lookup during Inline Parsing

In `inlines.c`, when a potential reference link is found:

```c
cmark_reference *ref = cmark_reference_lookup(subj->refmap, &raw_label);
if (ref) {
  // Create link node with ref->url and ref->title
}
```

## Label Length Limit

```c
#define MAX_LINK_LABEL_LENGTH 999
```

Reference labels longer than 999 characters are rejected, per the CommonMark spec.

## Map Lifecycle

```c
cmark_reference_map *cmark_reference_map_new(cmark_mem *mem);
void cmark_reference_map_free(cmark_reference_map *map);
```

The map is created during parser initialization and freed when the parser is freed. The AST's reference links have already been resolved and store their own copies of URL and title — the reference map is not needed after parsing.

### Cleanup

```c
void cmark_reference_map_free(cmark_reference_map *map) {
  if (map == NULL) return;
  for (unsigned int i = 0; i < map->size; i++) {
    cmark_reference_free(map->mem, map->refs[i]);
  }
  map->mem->free(map->refs);
  map->mem->free(map);
}
```

Each reference and its strings (label, url, title) are freed, then the array and map struct are freed.

## Cross-References

- [references.c](../../cmark/src/references.c) — Implementation
- [references.h](../../cmark/src/references.h) — Data structures
- [block-parsing.md](block-parsing.md) — Reference extraction during paragraph finalization
- [inline-parsing.md](inline-parsing.md) — Reference lookup during link resolution
- [utf8-handling.md](utf8-handling.md) — Case folding used in label normalization
