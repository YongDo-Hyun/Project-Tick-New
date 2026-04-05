# cmark — UTF-8 Handling

## Overview

The UTF-8 module (`utf8.c`, `utf8.h`) provides Unicode support for cmark: encoding, decoding, validation, iteration, case folding, and character classification. It incorporates data from `utf8proc` for case folding and character properties.

## UTF-8 Encoding Fundamentals

The module handles all four UTF-8 byte patterns:

| Codepoint Range | Byte 1 | Byte 2 | Byte 3 | Byte 4 |
|----------------|--------|--------|--------|--------|
| U+0000–U+007F | 0xxxxxxx | | | |
| U+0080–U+07FF | 110xxxxx | 10xxxxxx | | |
| U+0800–U+FFFF | 1110xxxx | 10xxxxxx | 10xxxxxx | |
| U+10000–U+10FFFF | 11110xxx | 10xxxxxx | 10xxxxxx | 10xxxxxx |

## Byte Classification Table

```c
static const uint8_t utf8proc_utf8class[256] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x00-0x0F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x10-0x1F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x20-0x2F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x30-0x3F
  // ...
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x80-0x8F (continuation)
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x90-0x9F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xA0-0xAF
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xB0-0xBF
  0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // 0xC0-0xCF
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // 0xD0-0xDF
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // 0xE0-0xEF
  4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xF0-0xFF
};
```

Lookup table that maps each byte to its UTF-8 sequence length:
- `1` → ASCII single-byte character
- `2` → Two-byte sequence lead byte (0xC2-0xDF)
- `3` → Three-byte sequence lead byte (0xE0-0xEF)
- `4` → Four-byte sequence lead byte (0xF0-0xF4)
- `0` → Continuation byte (0x80-0xBF) or invalid lead byte (0xC0-0xC1, 0xF5-0xFF)

Note: 0xC0 and 0xC1 are marked as `0` (invalid) because they would encode codepoints < 0x80, which is an overlong encoding.

## UTF-8 Encoding

```c
void cmark_utf8proc_encode_char(int32_t uc, cmark_strbuf *buf) {
  uint8_t dst[4];
  bufsize_t len = 0;

  assert(uc >= 0);

  if (uc < 0x80) {
    dst[0] = (uint8_t)(uc);
    len = 1;
  } else if (uc < 0x800) {
    dst[0] = (uint8_t)(0xC0 + (uc >> 6));
    dst[1] = 0x80 + (uc & 0x3F);
    len = 2;
  } else if (uc == 0xFFFF) {
    // Invalid codepoint — encode replacement char
    dst[0] = 0xEF; dst[1] = 0xBF; dst[2] = 0xBD;
    len = 3;
  } else if (uc == 0xFFFE) {
    // Invalid codepoint — encode replacement char
    dst[0] = 0xEF; dst[1] = 0xBF; dst[2] = 0xBD;
    len = 3;
  } else if (uc < 0x10000) {
    dst[0] = (uint8_t)(0xE0 + (uc >> 12));
    dst[1] = 0x80 + ((uc >> 6) & 0x3F);
    dst[2] = 0x80 + (uc & 0x3F);
    len = 3;
  } else if (uc < 0x110000) {
    dst[0] = (uint8_t)(0xF0 + (uc >> 18));
    dst[1] = 0x80 + ((uc >> 12) & 0x3F);
    dst[2] = 0x80 + ((uc >> 6) & 0x3F);
    dst[3] = 0x80 + (uc & 0x3F);
    len = 4;
  } else {
    // Out of range — encode replacement char U+FFFD
    dst[0] = 0xEF; dst[1] = 0xBF; dst[2] = 0xBD;
    len = 3;
  }

  cmark_strbuf_put(buf, dst, len);
}
```

Encodes a single Unicode codepoint as UTF-8 into a `cmark_strbuf`. Invalid codepoints (U+FFFE, U+FFFF, > U+10FFFF) are replaced with U+FFFD (replacement character).

## UTF-8 Validation and Iteration

```c
void cmark_utf8proc_check(cmark_strbuf *dest, const uint8_t *line,
                          bufsize_t size) {
  bufsize_t i = 0;

  while (i < size) {
    bufsize_t byte_length = utf8proc_utf8class[line[i]];
    int32_t codepoint = -1;

    if (byte_length == 0) {
      // Invalid lead byte — replace
      cmark_utf8proc_encode_char(0xFFFD, dest);
      i++;
      continue;
    }

    // Check we have enough bytes
    if (i + byte_length > size) {
      // Truncated sequence — replace
      cmark_utf8proc_encode_char(0xFFFD, dest);
      i++;
      continue;
    }

    // Decode and validate
    switch (byte_length) {
    case 1:
      codepoint = line[i];
      break;
    case 2:
      // Validate continuation byte
      if ((line[i+1] & 0xC0) != 0x80) { /* invalid */ }
      codepoint = ((line[i] & 0x1F) << 6) | (line[i+1] & 0x3F);
      break;
    case 3:
      // Validate continuation bytes + overlong + surrogates
      codepoint = ((line[i] & 0x0F) << 12) |
                  ((line[i+1] & 0x3F) << 6) |
                  (line[i+2] & 0x3F);
      // Reject surrogates (U+D800-U+DFFF) and overlongs
      break;
    case 4:
      // Validate continuation bytes + overlongs + max codepoint
      codepoint = ((line[i] & 0x07) << 18) |
                  ((line[i+1] & 0x3F) << 12) |
                  ((line[i+2] & 0x3F) << 6) |
                  (line[i+3] & 0x3F);
      break;
    }

    if (codepoint < 0) {
      cmark_utf8proc_encode_char(0xFFFD, dest);
      i++;
    } else {
      cmark_utf8proc_encode_char(codepoint, dest);
      i += byte_length;
    }
  }
}
```

This function validates UTF-8 and replaces invalid sequences with U+FFFD. It enforces:
- No invalid lead bytes
- No truncated sequences
- No invalid continuation bytes
- No overlong encodings
- No surrogate codepoints (U+D800-U+DFFF)

### Validation Rules (RFC 3629)

For 3-byte sequences:
```c
// Reject overlongs: first byte 0xE0 requires second byte >= 0xA0
if (line[i] == 0xE0 && line[i+1] < 0xA0) { /* overlong */ }
// Reject surrogates: first byte 0xED requires second byte < 0xA0
if (line[i] == 0xED && line[i+1] >= 0xA0) { /* surrogate */ }
```

For 4-byte sequences:
```c
// Reject overlongs: first byte 0xF0 requires second byte >= 0x90
if (line[i] == 0xF0 && line[i+1] < 0x90) { /* overlong */ }
// Reject codepoints > U+10FFFF: first byte 0xF4 requires second byte < 0x90
if (line[i] == 0xF4 && line[i+1] >= 0x90) { /* out of range */ }
```

## UTF-8 Iterator

```c
void cmark_utf8proc_iterate(const uint8_t *str, bufsize_t str_len,
                            int32_t *dst) {
  *dst = -1;
  if (str_len <= 0) return;

  uint8_t length = utf8proc_utf8class[str[0]];
  if (!length) return;
  if (str_len >= length) {
    switch (length) {
    case 1:
      *dst = str[0];
      break;
    case 2:
      *dst = ((int32_t)(str[0] & 0x1F) << 6) | (str[1] & 0x3F);
      break;
    case 3:
      *dst = ((int32_t)(str[0] & 0x0F) << 12) |
             ((int32_t)(str[1] & 0x3F) << 6) |
             (str[2] & 0x3F);
      // Reject surrogates:
      if (*dst >= 0xD800 && *dst < 0xE000) *dst = -1;
      break;
    case 4:
      *dst = ((int32_t)(str[0] & 0x07) << 18) |
             ((int32_t)(str[1] & 0x3F) << 12) |
             ((int32_t)(str[2] & 0x3F) << 6) |
             (str[3] & 0x3F);
      if (*dst > 0x10FFFF) *dst = -1;
      break;
    }
  }
}
```

Decodes a single UTF-8 codepoint from a byte string. Sets `*dst` to -1 on error.

## Case Folding

```c
void cmark_utf8proc_case_fold(cmark_strbuf *dest, const uint8_t *str,
                              bufsize_t len) {
  int32_t c;
  bufsize_t i = 0;

  while (i < len) {
    bufsize_t char_len = cmark_utf8proc_charlen(str + i, len - i);
    if (char_len < 0) {
      cmark_utf8proc_encode_char(0xFFFD, dest);
      i += 1;
      continue;
    }
    cmark_utf8proc_iterate(str + i, char_len, &c);
    if (c >= 0) {
      // Look up case fold mapping
      const int32_t *fold = cmark_utf8proc_case_fold_info(c);
      if (fold) {
        // Some characters fold to multiple codepoints
        while (*fold >= 0) {
          cmark_utf8proc_encode_char(*fold, dest);
          fold++;
        }
      } else {
        cmark_utf8proc_encode_char(c, dest);
      }
    }
    i += char_len;
  }
}
```

Performs Unicode case folding (not lowercasing — case folding is more aggressive and designed for case-insensitive comparison). Used for normalizing link reference labels.

### Case Fold Lookup

```c
static const int32_t *cmark_utf8proc_case_fold_info(int32_t c);
```

Uses a sorted table `cf_table` and binary search to find case fold mappings. Each entry maps a codepoint to one or more replacement codepoints (some characters fold to multiple characters, e.g., `ß` → `ss`).

The table uses sentinel value `-1` to terminate multi-codepoint sequences.

## Character Classification

### `cmark_utf8proc_is_space()`

```c
int cmark_utf8proc_is_space(int32_t c) {
  // ASCII spaces
  if (c < 0x80) {
    return (c == 9 || c == 10 || c == 12 || c == 13 || c == 32);
  }
  // Unicode Zs category
  return (c == 0xa0 || c == 0x1680 ||
          (c >= 0x2000 && c <= 0x200a) ||
          c == 0x202f || c == 0x205f || c == 0x3000);
}
```

Matches ASCII whitespace (HT, LF, FF, CR, SP) and Unicode Zs (space separator) characters including:
- U+00A0 (NBSP)
- U+1680 (Ogham space)
- U+2000-U+200A (various typographic spaces)
- U+202F (narrow NBSP)
- U+205F (medium mathematical space)
- U+3000 (ideographic space)

### `cmark_utf8proc_is_punctuation()`

```c
int cmark_utf8proc_is_punctuation(int32_t c) {
  // ASCII punctuation ranges
  if (c < 128) {
    return (c >= 33 && c <= 47) ||
           (c >= 58 && c <= 64) ||
           (c >= 91 && c <= 96) ||
           (c >= 123 && c <= 126);
  }
  // Unicode Pc, Pd, Pe, Pf, Pi, Po, Ps categories
  // Uses a table-driven approach for Unicode punctuation
}
```

Returns true for ASCII punctuation (`!`, `"`, `#`, `$`, `%`, `&`, `'`, `(`, `)`, `*`, `+`, `,`, `-`, `.`, `/`, `:`, `;`, `<`, `=`, `>`, `?`, `@`, `[`, `\\`, `]`, `^`, `_`, `` ` ``, `{`, `|`, `}`, `~`) and Unicode punctuation (categories Pc through Ps).

These classification functions are critical for inline parsing, specifically for delimiter run classification — determining whether a `*` or `_` run is left-flanking or right-flanking depends on whether adjacent characters are spaces or punctuation.

## Helper Functions

### `cmark_utf8proc_charlen()`

```c
static CMARK_INLINE bufsize_t cmark_utf8proc_charlen(const uint8_t *str,
                                                      bufsize_t str_len) {
  bufsize_t length = utf8proc_utf8class[str[0]];
  if (!length || str_len < length) return -length;
  return length;
}
```

Returns the byte length of the UTF-8 character at the given position. Returns negative on error (invalid byte or truncated).

## Usage in cmark

1. **Input validation**: `cmark_utf8proc_check()` is called on input to replace invalid UTF-8 with U+FFFD
2. **Reference normalization**: `cmark_utf8proc_case_fold()` is used by `normalize_reference()` in `references.c` for case-insensitive reference label matching
3. **Delimiter classification**: `cmark_utf8proc_is_space()` and `cmark_utf8proc_is_punctuation()` are used in `inlines.c` for the left-flanking/right-flanking delimiter run rules
4. **Entity decoding**: `cmark_utf8proc_encode_char()` is used when decoding HTML entities and numeric character references to produce their UTF-8 representation
5. **Renderer output**: `cmark_render_code_point()` in `render.c` calls `cmark_utf8proc_encode_char()` for multi-byte character output

## Cross-References

- [utf8.c](../../cmark/src/utf8.c) — Implementation
- [utf8.h](../../cmark/src/utf8.h) — Public interface
- [inline-parsing.md](inline-parsing.md) — Uses character classification for delimiter rules
- [reference-system.md](reference-system.md) — Uses case folding for label normalization
