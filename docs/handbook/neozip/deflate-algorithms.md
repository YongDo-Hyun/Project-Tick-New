# Deflate Algorithms

## Overview

The DEFLATE algorithm (RFC 1951) combines **LZ77** sliding-window compression
with **Huffman coding**. Neozip implements DEFLATE through a modular strategy
system where each compression level maps to a specific strategy function with
tuned parameters. This document covers every aspect of the compression
pipeline: hash chains, match finding, lazy evaluation, and the strategy
functions.

---

## The DEFLATE Compression Pipeline

At a high level, DEFLATE works as follows:

1. **Sliding window**: Input is processed through a 32KB (configurable)
   sliding window. Each position is hashed and inserted into a hash table.
2. **LZ77 match finding**: For each position, the hash table is consulted to
   find previous occurrences of the same byte sequence. The longest match
   within the window distance is selected.
3. **Symbol emission**: Either a **literal** byte (no match found) or a
   **length/distance pair** (match found) is recorded.
4. **Huffman coding**: Accumulated symbols are encoded with Huffman codes
   (static, dynamic, or the block is stored uncompressed) and output as a
   DEFLATE block.

```
Input bytes
    │
    ▼
┌──────────┐     ┌──────────────┐     ┌──────────────┐     ┌────────────┐
│  Sliding  │ ──▶ │ Hash Insert  │ ──▶ │ Match Finding│ ──▶ │  Symbol    │
│  Window   │     │ (head/prev)  │     │ (longest_    │     │  Buffer    │
│ (32K×2)   │     │              │     │  match)      │     │ (sym_buf)  │
└──────────┘     └──────────────┘     └──────────────┘     └─────┬──────┘
                                                                  │
                                                                  ▼
                                                           ┌────────────┐
                                                           │  Huffman   │
                                                           │  Encoding  │
                                                           │ (trees.c)  │
                                                           └─────┬──────┘
                                                                  │
                                                                  ▼
                                                           Compressed
                                                           Output
```

---

## The Sliding Window

The sliding window is a byte array of size `2 * w_size`, where `w_size`
defaults to 32768 (32KB, corresponding to `windowBits = 15`):

```c
unsigned char *window;    // Size: 2 * w_size bytes
unsigned int window_size; // = 2 * w_size
unsigned int w_size;      // = 1 << windowBits (default 32768)
```

Input is read into the **upper half** of the window (positions `w_size` to
`2*w_size - 1`). When this half fills up, the lower half is discarded, the
upper half is moved to the lower half (`memcpy` or `memmove`), and new input
fills the upper half again. This is the "slide" operation.

### Window Sliding

The `fill_window()` function in `deflate.c` performs the slide:

1. If more data is needed and `strstart >= w_size + MAX_DIST(s)`:
   - Copy bytes `[w_size, 2*w_size)` to `[0, w_size)`
   - Adjust `match_start`, `strstart`, `block_start` by `-w_size`
   - Call `FUNCTABLE_CALL(slide_hash)(s)` to update hash table entries
2. Read new input from `strm->next_in` into the free space
3. Maintain the `high_water` mark for memory-check safety

### `slide_hash()`

When the window slides, all hash table entries (`head[]` and `prev[]`) must be
decremented by `w_size`. Entries that would become negative (pointing before
the new window start) are set to zero.

The generic C implementation:
```c
void slide_hash_c(deflate_state *s) {
    Pos *p;
    unsigned n = HASH_SIZE;
    p = &s->head[n];
    do {
        unsigned m = *--p;
        *p = (Pos)(m >= w_size ? m - w_size : 0);
    } while (--n);
    // Same for prev[]
}
```

SIMD implementations process 8 or 16 entries at a time using vector
subtraction and saturation.

---

## Hash Table Structure

The hash table maps byte sequences to window positions for fast match lookup.
It consists of two arrays:

### `head[HASH_SIZE]`

An array of `Pos` (uint16_t) values, indexed by hash value. Each entry
contains the most recent window position that hashed to that index.

```c
#define HASH_BITS  16u
#define HASH_SIZE  65536u       // 2^16
#define HASH_MASK  (HASH_SIZE - 1u)

Pos *head;   // head[HASH_SIZE]: most recent position for each hash
```

Note: zlib-ng uses a 16-bit hash (65536 entries) compared to original zlib's
15-bit hash (32768 entries), providing better distribution and fewer collisions.

### `prev[w_size]`

An array maintaining the hash **chain** — a linked list of all positions
that share the same hash value:

```c
Pos *prev;   // prev[w_size]: chain links, indexed by (position & w_mask)
```

When a new position `str` is inserted for hash value `h`:
```c
prev[str & w_mask] = head[h];   // Link to previous chain head
head[h] = str;                  // New chain head
```

Walking the chain from `head[h]` through `prev[]` yields all previous
positions with the same hash, in reverse chronological order.

---

## Hash Function

Neozip hashes 4 bytes at the current position (compared to zlib's 3 bytes).
The hash function is defined in `insert_string_tpl.h`:

```c
Z_FORCEINLINE static uint32_t UPDATE_HASH(uint32_t h, uint32_t val) {
    HASH_CALC(h, val);          // Architecture-specific hash computation
    return h & HASH_CALC_MASK;  // Mask to HASH_SIZE
}
```

The `HASH_CALC` macro uses a fast multiplicative hash or CRC-based hash
depending on the configuration. Reading 4 bytes at once (`zng_memread_4`)
provides better hash distribution than 3-byte hashing.

### Hash Insert Operations

Several insert variants exist:

#### `quick_insert_value()`

Used by `deflate_fast` and `deflate_quick`. Inserts a single position
with a pre-read 4-byte value:

```c
Z_FORCEINLINE static uint32_t QUICK_INSERT_VALUE(
        deflate_state *const s, uint32_t str, uint32_t val) {
    uint32_t hm, head;
    HASH_CALC_VAR_INIT;
    HASH_CALC(HASH_CALC_VAR, val);
    HASH_CALC_VAR &= HASH_CALC_MASK;
    hm = HASH_CALC_VAR;
    head = s->head[hm];
    if (LIKELY(head != str)) {
        s->prev[str & W_MASK(s)] = (Pos)head;
        s->head[hm] = (Pos)str;
    }
    return head;
}
```

#### `quick_insert_string()`

Like `quick_insert_value()` but reads the 4 bytes itself:

```c
Z_FORCEINLINE static uint32_t QUICK_INSERT_STRING(
        deflate_state *const s, uint32_t str) {
    uint8_t *strstart = s->window + str + HASH_CALC_OFFSET;
    uint32_t val, hm, head;
    HASH_CALC_VAR_INIT;
    HASH_CALC_READ;   // val = Z_U32_FROM_LE(zng_memread_4(strstart))
    HASH_CALC(HASH_CALC_VAR, val);
    // ... insert ...
    return head;
}
```

#### `insert_string()`

Batch insert. Inserts `count` consecutive positions, used after a match
to insert all strings within the matched region:

```c
void insert_string(deflate_state *const s, uint32_t str, uint32_t count);
```

#### `insert_string_roll()` / `quick_insert_string_roll()`

Rolling hash insert for level 9 (`deflate_slow` with `LONGEST_MATCH_SLOW`).
Uses a different hash update that considers the full string context for
better match quality at the cost of speed.

---

## Match Finding

### `longest_match()` (Standard)

Defined via the `match_tpl.h` template. This is the hot inner loop of
compression.

**Algorithm**:

1. Set `best_len` to `prev_length` (or `STD_MIN_MATCH - 1`)
2. Read the first 8 bytes at `scan` and at `scan + offset` for fast
   comparison (where `offset = best_len - 1`, adjusted for word boundaries)
3. Set `limit` to prevent matches beyond `MAX_DIST(s)`
4. If `best_len >= good_match`, halve `chain_length` to speed up
5. Walk the hash chain via `prev[]`:
   a. For each candidate `cur_match`:
      - Quick reject: compare 8 bytes at match end position (`mbase_end + cur_match`)
        against `scan_end`
      - Quick reject: compare 8 bytes at match start against `scan_start`
      - If both match: call `compare256()` for exact length
      - If new length > `best_len`: update `best_len` and `match_start`
      - If `best_len >= nice_match`: return immediately
      - Update `scan_end` for the new best length
   b. Follow `prev[cur_match & wmask]` to next chain entry
   c. Stop when `chain_length` exhausted or `cur_match <= limit`

```c
Z_INTERNAL uint32_t LONGEST_MATCH(deflate_state *const s, uint32_t cur_match) {
    // Early exit: if chain becomes too deep for poor matches
    int32_t early_exit = s->level < EARLY_EXIT_TRIGGER_LEVEL;

    while (--chain_length) {
        match = mbase_start + cur_match;
        if (zng_memread_8(mbase_end + cur_match) == scan_end &&
            zng_memread_8(match) == scan_start) {
            len = FUNCTABLE_CALL(compare256)(scan + 2, match + 2) + 2;
            if (len > best_len) {
                s->match_start = cur_match;
                best_len = len;
                if (best_len >= nice_match) return best_len;
                offset = best_len - 1;
                // Update scan_end
            }
        }
        cur_match = prev[cur_match & wmask];
        if (cur_match <= limit) return best_len;
    }
}
```

**Key parameters** that control match-finding behaviour:

| Parameter | Field | Effect |
|---|---|---|
| `max_chain_length` | `s->max_chain_length` | Maximum hash chain entries to check |
| `good_match` | `s->good_match` | Halve chain search above this best length |
| `nice_match` | `s->nice_match` | Stop searching above this length |
| `max_lazy_match` | `s->max_lazy_match` | Don't bother with lazy match above this |

### `longest_match_slow()` (Level 9)

The slow variant (`LONGEST_MATCH_SLOW`) adds chain re-rooting for better
match quality:

1. When continuing a lazy evaluation search, it doesn't just follow the
   chain from the hash of the current position
2. Instead, it finds the most distant chain starting from positions
   `scan[1], scan[2], ..., scan[best_len]` using `update_hash_roll()`
3. This effectively searches multiple hash chains to find matches that
   the standard algorithm would miss

```c
// Re-root: find a more distant chain start
hash = update_hash_roll(0, scan[1]);
hash = update_hash_roll(hash, scan[2]);
for (uint32_t i = 3; i <= best_len; i++) {
    hash = update_hash_roll(hash, scan[i]);
    pos = s->head[hash];
    if (pos < cur_match) {
        cur_match = pos;  // Found a more distant starting point
    }
}
```

### `compare256()`

Compares up to 256 bytes between two pointers, returning the number of
matching bytes. Architecture-specific implementations:

| Implementation | File | Method |
|---|---|---|
| `compare256_c` | `arch/generic/compare256_c.c` | 8-byte word comparison |
| `compare256_sse2` | `arch/x86/compare256_sse2.c` | 16-byte SSE2 comparison |
| `compare256_avx2` | `arch/x86/compare256_avx2.c` | 32-byte AVX2 comparison |
| `compare256_avx512` | `arch/x86/compare256_avx512.c` | 64-byte AVX-512 comparison |
| `compare256_neon` | `arch/arm/compare256_neon.c` | 16-byte NEON comparison |

The compare function uses `FUNCTABLE_CALL(compare256)` for dispatch.

---

## Configuration Table

Each compression level has a set of tuning parameters defined in
`deflate.c`:

```c
typedef struct config_s {
    uint16_t good_length; // Reduce lazy search above this match length
    uint16_t max_lazy;    // Do not perform lazy search above this length
    uint16_t nice_length; // Quit search above this match length
    uint16_t max_chain;   // Maximum hash chain length
    compress_func func;   // Strategy function pointer
} config;
```

The full table:

| Level | good | lazy | nice | chain | Strategy |
|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 | `deflate_stored` |
| 1 | 0 | 0 | 0 | 0 | `deflate_quick` |
| 2 | 4 | 4 | 8 | 4 | `deflate_fast` |
| 3 | 4 | 6 | 16 | 6 | `deflate_medium` |
| 4 | 4 | 12 | 32 | 24 | `deflate_medium` |
| 5 | 8 | 16 | 32 | 32 | `deflate_medium` |
| 6 | 8 | 16 | 128 | 128 | `deflate_medium` |
| 7 | 8 | 32 | 128 | 256 | `deflate_slow` |
| 8 | 32 | 128 | 258 | 1024 | `deflate_slow` |
| 9 | 32 | 258 | 258 | 4096 | `deflate_slow` |

When `NO_QUICK_STRATEGY` is defined, level 1 uses `deflate_fast` and level 2
shifts accordingly. When `NO_MEDIUM_STRATEGY` is defined, levels 3–6 use
`deflate_fast` or `deflate_slow`.

---

## Strategy Functions in Detail

### `deflate_stored` (Level 0)

No compression. Input is emitted as stored blocks (type 0 in DEFLATE):

- Each stored block has a 5-byte header: BFINAL (1 bit), BTYPE (2 bits = 00),
  padding to byte boundary, LEN (16 bits), NLEN (16 bits, one's complement of LEN)
- Maximum stored block length: 65535 bytes (`MAX_STORED`)
- Directly copies from `next_in` to `next_out` when possible
- Falls back to buffering through the window when direct copy isn't feasible

```c
Z_INTERNAL block_state deflate_stored(deflate_state *s, int flush) {
    unsigned min_block = MIN(s->pending_buf_size - 5, w_size);
    // ...
    len = MAX_STORED;
    have = (s->bi_valid + 42) >> 3;  // Header overhead
    // Copy blocks directly when possible
}
```

### `deflate_quick` (Level 1)

The fastest compression strategy, designed by Intel:

- Uses **static Huffman trees** only (no dynamic tree construction)
- Single-pass greedy matching with no lazy evaluation
- Emits blocks via `zng_tr_emit_tree(s, STATIC_TREES, last)` and
  `zng_tr_emit_end_block(s, static_ltree, last)`
- Tracks block state: `block_open` = 0 (closed), 1 (open), 2 (open + last)
- Flushes when `pending` approaches `pending_buf_size`

```c
Z_INTERNAL block_state deflate_quick(deflate_state *s, int flush) {
    // Start static block
    quick_start_block(s, last);

    for (;;) {
        // Flush if pending buffer nearly full
        if (s->pending + ((BIT_BUF_SIZE + 7) >> 3) >= s->pending_buf_size) {
            PREFIX(flush_pending)(s->strm);
            // ...
        }

        // Hash insert
        uint32_t hash_head = quick_insert_value(s, s->strstart, str_val);

        // Try to find a match
        if (dist <= MAX_DIST(s) && dist > 0) {
            // Quick match comparison...
            if (match_val == str_val) {
                // Full match via longest_match or inline compare
                zng_tr_emit_dist(s, static_ltree, static_dtree, ...);
            }
        }
        // No match: emit literal
        zng_tr_emit_lit(s, static_ltree, lc);
    }
}
```

### `deflate_fast` (Level 2, or 1–3 without quick)

Greedy matching without lazy evaluation:

1. `fill_window()` if `lookahead < MIN_LOOKAHEAD`
2. Hash insert via `quick_insert_value()`
3. If match found (within `MAX_DIST`, length ≥ `WANT_MIN_MATCH`):
   - Record match via `zng_tr_tally_dist()`
   - Insert all strings within the match region
   - Advance `strstart` by `match_length`
4. If no match:
   - Record literal via `zng_tr_tally_lit()`
   - Advance `strstart` by 1
5. If tally function returns true (buffer full): flush block

```c
Z_INTERNAL block_state deflate_fast(deflate_state *s, int flush) {
    for (;;) {
        if (s->lookahead < MIN_LOOKAHEAD) {
            PREFIX(fill_window)(s);
            // ...
        }
        if (s->lookahead >= WANT_MIN_MATCH) {
            uint32_t hash_head = quick_insert_value(s, s->strstart, str_val);
            if (dist <= MAX_DIST(s) && dist > 0 && hash_head != 0) {
                match_len = FUNCTABLE_CALL(longest_match)(s, hash_head);
            }
        }
        if (match_len >= WANT_MIN_MATCH) {
            bflush = zng_tr_tally_dist(s, s->strstart - s->match_start,
                                        match_len - STD_MIN_MATCH);
            s->lookahead -= match_len;
            // Insert strings within match
            s->strstart += match_len;
        } else {
            bflush = zng_tr_tally_lit(s, lc);
            s->lookahead--;
            s->strstart++;
        }
        if (UNLIKELY(bflush))
            FLUSH_BLOCK(s, 0);
    }
}
```

### `deflate_medium` (Levels 3–6)

Intel's balanced strategy that bridges fast and slow:

Uses a `struct match` to track match attributes:
```c
struct match {
    uint16_t match_start;
    uint16_t match_length;
    uint16_t strstart;
    uint16_t orgstart;
};
```

Key helper functions:
- **`find_best_match()`** — Calls `longest_match()` and returns a `struct match`
- **`emit_match()`** — Emits literals for short matches (< `WANT_MIN_MATCH`)
  or a length/distance pair for longer ones
- **`insert_match()`** — Inserts hash entries for matched positions

The algorithm maintains a two-position lookahead:

1. Find best match at current position
2. Find best match at next position
3. Compare: if next match is better by a meaningful margin, emit current
   position as a literal and adopt the next match
4. Otherwise, emit the current match

```c
Z_INTERNAL block_state deflate_medium(deflate_state *s, int flush) {
    struct match current_match, next_match;

    for (;;) {
        current_match = find_best_match(s, hash_head);

        if (current_match.match_length >= WANT_MIN_MATCH) {
            // Check if next position has a better match
            next_match = find_best_match(s, next_hash_head);
            if (next_match.match_length > current_match.match_length + 1) {
                // Skip current, use next
                emit_match(s, literal_match);
                insert_match(s, literal_match);
                continue;
            }
        }
        emit_match(s, current_match);
        insert_match(s, current_match);
    }
}
```

### `deflate_slow` (Levels 7–9)

Full lazy match evaluation — the traditional approach for maximum compression:

1. Find longest match at current position
2. **Do not emit it yet** — set `match_available = 1`
3. Advance to next position, find another match
4. If the new match is **not better** than the previous one:
   - Emit the previous match (the "lazy" match)
   - Insert all strings within the matched region
   - Skip past the match
5. If the new match **is better**:
   - Emit the previous position as a literal
   - Record the new match as `match_available`
   - Continue from step 3

```c
Z_INTERNAL block_state deflate_slow(deflate_state *s, int flush) {
    // Level ≥ 9: use slow match finder and rolling insert
    if (level >= 9) {
        longest_match = FUNCTABLE_FPTR(longest_match_slow);
        insert_string_func = insert_string_roll;
    }

    for (;;) {
        // Find match
        if (dist <= MAX_DIST(s) && s->prev_length < s->max_lazy_match) {
            match_len = longest_match(s, hash_head);
        }

        // Lazy evaluation
        if (s->prev_length >= STD_MIN_MATCH && match_len <= s->prev_length) {
            // Previous match was better — emit it
            bflush = zng_tr_tally_dist(s, s->strstart - 1 - s->prev_match,
                                        s->prev_length - STD_MIN_MATCH);
            // Insert strings within match
            s->prev_length -= 1;
            s->lookahead -= s->prev_length;
            s->strstart += s->prev_length;
            s->prev_length = 0;
            s->match_available = 0;
        } else if (s->match_available) {
            // Previous position has no good match — emit as literal
            bflush = zng_tr_tally_lit(s, s->window[s->strstart - 1]);
        } else {
            s->match_available = 1;
        }

        s->prev_length = match_len;
        s->prev_match = s->match_start;
    }
}
```

**Level 9 enhancements**:
- Uses `longest_match_slow` which re-roots hash chains for deeper search
- Uses `insert_string_roll` with a rolling hash for better distribution
- `max_chain = 4096` provides the deepest chain traversal
- `nice_match = 258` (maximum match length) means it never gives up early

### `deflate_huff` (Z_HUFFMAN_ONLY)

Huffman-only compression — every byte is emitted as a literal, no LZ77:

```c
Z_INTERNAL block_state deflate_huff(deflate_state *s, int flush) {
    for (;;) {
        if (s->lookahead == 0) {
            PREFIX(fill_window)(s);
            if (s->lookahead == 0) {
                if (flush == Z_NO_FLUSH) return need_more;
                break;
            }
        }
        bflush = zng_tr_tally_lit(s, s->window[s->strstart]);
        s->lookahead--;
        s->strstart++;
        if (bflush) FLUSH_BLOCK(s, 0);
    }
}
```

This forces construction of a Huffman tree that encodes only literal
frequencies. Useful when the data is already compressed or random.

### `deflate_rle` (Z_RLE)

Run-length encoding — only finds runs of identical bytes (distance = 1):

```c
Z_INTERNAL block_state deflate_rle(deflate_state *s, int flush) {
    for (;;) {
        // Check for a run: scan[-1] == scan[0] == scan[1]
        if (s->lookahead >= STD_MIN_MATCH && s->strstart > 0) {
            scan = s->window + s->strstart - 1;
            if (scan[0] == scan[1] && scan[1] == scan[2]) {
                match_len = compare256_rle(scan, scan + 3) + 2;
                match_len = MIN(match_len, STD_MAX_MATCH);
            }
        }

        if (match_len >= STD_MIN_MATCH) {
            bflush = zng_tr_tally_dist(s, 1, match_len - STD_MIN_MATCH);
        } else {
            bflush = zng_tr_tally_lit(s, s->window[s->strstart]);
        }
    }
}
```

The `compare256_rle()` function is optimised for the RLE case where
all bytes in the run are identical:

```c
// compare256_rle.h
static inline uint32_t compare256_rle_64(const uint8_t *src0, const uint8_t *src1) {
    // Read 8-byte words from src0, replicate src1[0] across 8 bytes
    // XOR and count trailing zeros to find first difference
}
```

---

## Symbol Buffer

Matches and literals are stored in a symbol buffer before being emitted:

### Overlaid Format (`sym_buf`, default)

Three bytes per symbol:
- `sym_buf[i+0]`, `sym_buf[i+1]` — distance (0 for literal)
- `sym_buf[i+2]` — literal byte or match length

```c
// zng_tr_tally_dist:
zng_memwrite_4(&s->sym_buf[sym_next], Z_U32_TO_LE(dist | ((uint32_t)len << 16)));
s->sym_next = sym_next + 3;

// zng_tr_tally_lit:
zng_memwrite_4(&s->sym_buf[sym_next], Z_U32_TO_LE((uint32_t)c << 16));
s->sym_next = sym_next + 3;
```

### Separate Format (`LIT_MEM`)

When `LIT_MEM` is defined (automatic when `OPTIMAL_CMP < 32`):
```c
uint16_t *d_buf;       // Distance buffer
unsigned char *l_buf;  // Literal/length buffer

// zng_tr_tally_dist:
s->d_buf[sym_next] = (uint16_t)dist;
s->l_buf[sym_next] = (uint8_t)len;
s->sym_next = sym_next + 1;
```

This uses ~20% more memory but is 1–2% faster on platforms without fast
unaligned access.

The buffer size is `lit_bufsize` entries. When `sym_next` reaches `sym_end`,
the block is flushed. The constant `LIT_BUFS` determines the buffer
multiplier: 4 (overlaid) or 5 (separate).

---

## Block Flushing

`zng_tr_flush_block()` in `trees.c` decides how to emit the accumulated
symbols:

1. **Compute tree statistics**: Build dynamic Huffman trees, compute
   `opt_len` (dynamic tree bit cost) and `static_len` (static tree bit cost)
2. **Compute stored cost**: Raw data length + 5 bytes overhead per block
3. **Choose the best**:
   - If stored cost ≤ `opt_len` and stored cost ≤ `static_len`: emit stored block
   - Else if `static_len` ≤ `opt_len + 10`: emit with static trees
   - Else: emit with dynamic trees

```c
void zng_tr_flush_block(deflate_state *s, char *buf, uint32_t stored_len, int last) {
    build_tree(s, &s->l_desc);
    build_tree(s, &s->d_desc);

    if (stored_len + 4 <= opt_len && buf != NULL) {
        // Stored block
    } else if (s->strategy == Z_FIXED || static_len == opt_len) {
        // Static trees
        compress_block(s, static_ltree, static_dtree);
    } else {
        // Dynamic trees
        send_all_trees(s, lcodes, dcodes, blcodes);
        compress_block(s, s->dyn_ltree, s->dyn_dtree);
    }
    init_block(s);  // Reset for next block
}
```

---

## Match Length Constraints

| Constant | Value | Purpose |
|---|---|---|
| `STD_MIN_MATCH` | 3 | Minimum match length per DEFLATE spec |
| `STD_MAX_MATCH` | 258 | Maximum match length per DEFLATE spec |
| `WANT_MIN_MATCH` | 4 | Internal minimum for actual match output |
| `MIN_LOOKAHEAD` | `STD_MAX_MATCH + WANT_MIN_MATCH + 1` | Minimum lookahead before refilling window |
| `MAX_DIST(s)` | `s->w_size - MIN_LOOKAHEAD` | Maximum back-reference distance |

`WANT_MIN_MATCH = 4` is a performance optimisation: 3-byte matches provide
minimal compression benefit but cost significant CPU time to find. By
requiring 4-byte matches, the hash function can read a full 32-bit word
and the match finder can use 32-bit comparisons for faster rejection.

---

## Block Types in Deflate Output

Every DEFLATE block starts with a 3-bit header:
- Bit 0: `BFINAL` — 1 if this is the last block
- Bits 1–2: `BTYPE` — block type (00=stored, 01=fixed, 10=dynamic, 11=reserved)

### Stored Block (BTYPE=00)

```
BFINAL BTYPE  pad   LEN    NLEN   DATA
  1     00    0-7   16     16     LEN bytes
```

### Fixed Huffman Block (BTYPE=01)

Uses predefined static trees (`static_ltree`, `static_dtree`). No tree
data in the block — the decoder knows the fixed codes.

### Dynamic Huffman Block (BTYPE=10)

Contains the tree definition before the data:
```
BFINAL BTYPE HLIT HDIST HCLEN [Code lengths for code length alphabet]
[Encoded literal/length code lengths] [Encoded distance code lengths]
[Compressed data using the defined trees]
```

---

## Flush Modes

The `flush` parameter to `deflate()` controls output behaviour:

| Value | Name | Effect |
|---|---|---|
| 0 | `Z_NO_FLUSH` | Normal operation; deflate decides when to emit output |
| 1 | `Z_PARTIAL_FLUSH` | Flush output, emit empty fixed code block (10 bits) |
| 2 | `Z_SYNC_FLUSH` | Flush output, align to byte, emit stored empty block (00 00 FF FF) |
| 3 | `Z_FULL_FLUSH` | Like `Z_SYNC_FLUSH` but also resets compression state for random access |
| 4 | `Z_FINISH` | Complete the stream. Returns `Z_STREAM_END` when done |
| 5 | `Z_BLOCK` | Stop at next block boundary |
| 6 | `Z_TREES` | Like `Z_BLOCK` but also emit trees (for `Z_TREES` flush) |

Flush priority is ordered via the `RANK()` macro:
```c
#define RANK(f) (((f) * 2) - ((f) > 4 ? 9 : 0))
```

---

## The `block_state` Return Values

Each strategy function returns a `block_state`:

```c
typedef enum {
    need_more,       // Need more input or more output space
    block_done,      // Block was flushed
    finish_started,  // Z_FINISH started, need more output calls
    finish_done      // Z_FINISH complete
} block_state;
```

`deflate()` uses these to control its outer loop and determine when to
return to the caller.
