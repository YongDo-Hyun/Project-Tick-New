# Neozip Architecture

## Overview

Neozip is structured as a layered compression library. At the highest level,
public API functions in `compress.c`, `uncompr.c`, `gzlib.c`, `gzread.c`, and
`gzwrite.c` provide simple interfaces. These delegate to the core streaming
engine implemented in `deflate.c` and `inflate.c`. The streaming engine in
turn relies on Huffman tree management (`trees.c`), hash table operations
(`insert_string.c`), match finding (`match_tpl.h`), and integrity checking
(`adler32.c`, `crc32.c`). At the bottom layer, architecture-specific SIMD
implementations in `arch/` provide hardware-accelerated versions of
performance-critical functions, selected at runtime by the dispatch table in
`functable.c`.

---

## Module Dependency Graph

```
Application
    │
    ├─── compress.c / uncompr.c          (one-shot API)
    ├─── gzlib.c / gzread.c / gzwrite.c  (gzip file I/O)
    │
    └─── deflate.c ──────── inflate.c     (streaming core)
         │                  │
         ├── deflate_fast.c │
         ├── deflate_quick.c│
         ├── deflate_medium.c
         ├── deflate_slow.c │
         ├── deflate_stored.c
         ├── deflate_huff.c │
         ├── deflate_rle.c  │
         │                  ├── inftrees.c  (code table builder)
         │                  ├── infback.c   (callback inflate)
         │                  │
         ├── trees.c ───────┘
         │
         ├── insert_string.c / match_tpl.h
         │
         ├── adler32.c ──── crc32.c       (checksums)
         │
         └── functable.c ── cpu_features.c (dispatch)
                            │
                            └── arch/
                                ├── generic/
                                ├── x86/
                                ├── arm/
                                ├── power/
                                ├── s390/
                                ├── riscv/
                                └── loongarch/
```

---

## Source File Reference

### Public API Layer

#### `compress.c`

Implements the one-shot `compress()` and `compress2()` functions. These
create a temporary `z_stream`, call `deflateInit()`, feed all input via a
single `deflate(Z_FINISH)` loop, and call `deflateEnd()`. The function
also provides `compressBound()` which returns the maximum compressed size
for a given input length.

```c
z_int32_t Z_EXPORT PREFIX(compress2)(
    unsigned char *dest, z_uintmax_t *destLen,
    const unsigned char *source, z_uintmax_t sourceLen,
    z_int32_t level);
```

The `DEFLATE_BOUND_COMPLEN` macro allows architecture-specific overrides
(used by IBM z DFLTCC).

#### `uncompr.c`

Implements `uncompress()` and `uncompress2()`. Creates a temporary `z_stream`,
calls `inflateInit()`, feeds all input, returns the decompressed data. If
`*destLen` is zero, a 1-byte dummy buffer is used to detect incomplete streams.

```c
z_int32_t Z_EXPORT PREFIX(uncompress2)(
    unsigned char *dest, z_uintmax_t *destLen,
    const unsigned char *source, z_uintmax_t *sourceLen);
```

#### `gzlib.c`

Common code for gzip file operations. Manages the `gz_state` structure
(defined in `gzguts.h`), which wraps a `z_stream` with file descriptor,
buffers, and state tracking.

Key functions:
- `gz_state_init()` — Allocates and zeroes a `gz_state`
- `gz_reset()` — Resets read/write state
- `gz_buffer_alloc()` — Allocates aligned I/O buffers (input is doubled for
  write mode, output is doubled for read mode)
- `gz_open()` — Opens a file, detects mode, initialises state
- `PREFIX(gzerror)()` — Returns error string and code
- `PREFIX(gzclearerr)()` — Clears error and EOF flags

The gzip file state (`gz_state`) tracks:
```c
typedef struct {
    struct gzFile_s x;     // exposed: have, next, pos
    int mode;              // GZ_NONE, GZ_READ, GZ_WRITE
    int fd;                // file descriptor
    char *path;            // for error messages
    unsigned size;         // buffer size (0 = not allocated yet)
    unsigned want;         // requested buffer size (default GZBUFSIZE=131072)
    unsigned char *in;     // input buffer
    unsigned char *out;    // output buffer
    int direct;            // 0=gzip, 1=transparent copy
    int how;               // LOOK, COPY, GZIP
    z_off64_t start;       // where gzip data starts
    int eof;               // end of input file reached
    int past;              // read past end
    int level;             // compression level
    int strategy;          // compression strategy
    // ...
} gz_state;
```

#### `gzread.c`

Implements `gzread()`, `gzgets()`, `gzgetc()`, `gzungetc()`, `gzclose_r()`.
Uses a lazy initialization pattern — buffers and inflate state are not
allocated until the first read.

The read pipeline:
1. `gz_look()` — Detect gzip header or transparent mode
2. `gz_decomp()` — Call `inflate()` to decompress
3. `gz_fetch()` — Fill output buffer
4. `gz_read()` — Copy from output buffer to user buffer

#### `gzwrite.c`

Implements `gzwrite()`, `gzprintf()`, `gzputc()`, `gzputs()`, `gzflush()`,
`gzclose_w()`. Write initialisation is also lazy.

The write pipeline:
1. `gz_write_init()` — Allocate buffers, call `deflateInit2()` with `MAX_WBITS + 16`
   (gzip wrapper)
2. `gz_comp()` — Call `deflate()` to compress, write to file descriptor
3. `gz_zero()` — Seek support: write zero bytes as padding

---

### Core Compression Engine

#### `deflate.c`

The central compression module. Contains:

- **`deflateInit2()`** — Main initialisation. Allocates the unified buffer via
  `alloc_deflate()`, initialises the `deflate_state`, sets up the hash table
  and window, selects the strategy function based on level.

- **`deflate()`** — Main compression entry point. Handles the state machine
  for header emission (zlib or gzip), calls the selected strategy function
  to process input, manages flushing and stream completion.

- **`deflateEnd()`** — Frees all resources via `free_deflate()`.

- **`deflateReset()`** — Resets state for reuse without reallocation.

- **`deflateParams()`** — Changes compression level and strategy mid-stream.
  Flushes the current block if the strategy function changes.

- **`deflateBound()`** — Returns worst-case compressed size.

- **`deflatePending()`** — Reports pending output bytes.

- **`deflateSetDictionary()`** — Loads a preset dictionary.

- **`deflateCopy()`** — Deep-copies a deflate stream.

- **`fill_window()`** — Slides the window and reads input into the available
  space. Updates hash entries via `slide_hash()`.

- **`alloc_deflate()`** — Single-allocation buffer partitioning.

- **`free_deflate()`** — Single-free cleanup.

- **`lm_init()`** — Initialises match-finding parameters from the
  `configuration_table`.

- **`lm_set_level()`** — Updates parameters when level changes.

##### The `deflate_state` Structure

The `internal_state` (aliased as `deflate_state`) is the central data
structure, carefully laid out for cache performance with `ALIGNED_(64)`:

```c
struct ALIGNED_(64) internal_state {
    // Cacheline 0
    PREFIX3(stream) *strm;           // Back-pointer to z_stream
    unsigned char   *pending_buf;    // Output pending buffer
    unsigned char   *pending_out;    // Next pending byte to output
    uint32_t         pending_buf_size;
    uint32_t         pending;        // Bytes in pending buffer
    int              wrap;           // bit 0: zlib, bit 1: gzip
    uint32_t         gzindex;        // Position in gzip extra/name/comment
    PREFIX(gz_headerp) gzhead;       // gzip header info
    int              status;         // INIT_STATE, GZIP_STATE, BUSY_STATE, etc.
    int              last_flush;
    int              reproducible;

    // Cacheline 1
    unsigned int     lookahead;      // Valid bytes ahead in window
    unsigned int     strstart;       // Start of string to insert
    unsigned int     w_size;         // LZ77 window size (32K default)
    int              block_start;    // Window position at block start
    unsigned int     high_water;     // Initialised bytes high water mark
    unsigned int     window_size;    // 2 * w_size
    unsigned char   *window;         // Sliding window
    Pos             *prev;           // Hash chain links
    Pos             *head;           // Hash chain heads
    uint32_t         ins_h;          // Hash index of string

    // Match state
    unsigned int     match_length;   // Best match length
    int              match_available;// Previous match exists flag
    uint32_t         prev_match;     // Previous match position
    unsigned int     match_start;    // Start of matching string
    unsigned int     prev_length;    // Best match at previous step
    unsigned int     max_chain_length;
    unsigned int     max_lazy_match;

    // Parameters
    int              level;
    int              strategy;
    unsigned int     good_match;
    int              nice_match;
    unsigned int     matches;
    unsigned int     insert;

    // Bit buffer
    uint64_t         bi_buf;         // 64-bit output buffer
    int32_t          bi_valid;       // Valid bits in bi_buf

    // Huffman trees
    struct ct_data_s dyn_ltree[HEAP_SIZE];   // Literal/length tree
    struct ct_data_s dyn_dtree[2*D_CODES+1]; // Distance tree
    struct ct_data_s bl_tree[2*BL_CODES+1];  // Bit-length tree

    struct tree_desc_s l_desc, d_desc, bl_desc;
    uint16_t bl_count[MAX_BITS+1];
    int      heap[2*L_CODES+1];
    unsigned char depth[2*L_CODES+1];

    // Symbol buffer
    unsigned int lit_bufsize;
#ifdef LIT_MEM
    uint16_t *d_buf;
    unsigned char *l_buf;
#else
    unsigned char *sym_buf;
#endif
    unsigned int sym_next;
    unsigned int sym_end;

    unsigned int opt_len;
    unsigned int static_len;
};
```

The state machine transitions are:

```
INIT_STATE → BUSY_STATE → FINISH_STATE
     ↓
GZIP_STATE → EXTRA_STATE → NAME_STATE → COMMENT_STATE → HCRC_STATE → BUSY_STATE
```

#### `deflate.h`

Defines the `deflate_state` (as `internal_state`), the `ct_data` union type
for Huffman tree nodes, the `tree_desc` descriptor, the `block_state` enum,
and all compression-related constants:

```c
#define LENGTH_CODES  29    // Number of length codes
#define LITERALS      256   // Number of literal bytes
#define L_CODES       (LITERALS+1+LENGTH_CODES)  // 286
#define D_CODES       30    // Number of distance codes
#define BL_CODES      19    // Bit-length codes
#define HEAP_SIZE     (2*L_CODES+1)  // 573
#define BIT_BUF_SIZE  64    // 64-bit bit buffer
#define END_BLOCK     256   // End-of-block literal code
#define HASH_BITS     16u
#define HASH_SIZE     65536u
```

#### `deflate_p.h`

Private inline functions shared across strategy files:

- **`zng_tr_tally_lit()`** — Records a literal byte. Stores into `sym_buf` or
  `l_buf`/`d_buf`, increments frequency counter.
- **`zng_tr_tally_dist()`** — Records a length/distance pair. Computes the
  length code via `zng_length_code[]` and distance code via `zng_dist_code[]`.
- **`check_match()`** — Debug-only match validation.
- **`flush_pending()`** — Copies pending output to `strm->next_out`.

The tally functions determine when a block should be flushed by checking
`sym_next == sym_end`.

---

### Strategy Implementations

#### `deflate_quick.c`

The **quick** strategy (level 1) was contributed by Intel. It uses **static
Huffman trees** exclusively (no dynamic tree construction), performing a
single-pass greedy match search.

Key characteristics:
- Emits and receives static tree blocks via `zng_tr_emit_tree()` /
  `zng_tr_emit_end_block()`
- Uses `quick_insert_value()` for single-shot hash insertion
- Match finding via `FUNCTABLE_CALL(longest_match)`
- No lazy evaluation
- Block management tracks `block_open` state (0=closed, 1=open, 2=open+last)

```c
Z_INTERNAL block_state deflate_quick(deflate_state *s, int flush) {
    // Emit static Huffman tree block header
    // For each position:
    //   - Hash insert current position
    //   - If match found: emit length/distance via static codes
    //   - Else: emit literal via static code
    // Flush when pending is near full
}
```

#### `deflate_fast.c`

The **fast** strategy (level 2, or 1–3 without quick) uses **greedy matching**
with no lazy evaluation:

1. Fill window if `lookahead < MIN_LOOKAHEAD`
2. Insert current string hash via `quick_insert_value()`
3. If hash chain has a match within `MAX_DIST`, call `longest_match()`
4. If match ≥ `WANT_MIN_MATCH` (4): emit distance/length, advance by match length,
   insert all strings within the match
5. Else: emit literal, advance by 1
6. Flush block when symbol buffer is full

#### `deflate_medium.c`

The **medium** strategy (levels 3–6) was contributed by Intel (Arjan van de Ven).
It provides a balance between speed and compression:

1. Two-match lookahead: finds the best match at the current position and the
   next position
2. Uses a `struct match` to track `match_start`, `match_length`, `strstart`,
   `orgstart`
3. `find_best_match()` — calls `longest_match()` and evaluates match quality
4. `emit_match()` — emits literals for short matches (< `WANT_MIN_MATCH`) or
   distance/length for longer ones
5. `insert_match()` — inserts hash entries for matched strings
6. Limited lazy evaluation based on comparing current and next match lengths

#### `deflate_slow.c`

The **slow** strategy (levels 7–9) performs full **lazy match evaluation**:

1. At each position, find the longest match
2. Instead of immediately emitting it, advance one position and find another match
3. If the new match is better, discard the previous one (lazy evaluation)
4. Level 9 uses `longest_match_slow` (the `LONGEST_MATCH_SLOW` variant) with
   `insert_string_roll` for deeper hash chain traversal

```c
// Level ≥ 9: use slow match and rolling insert
if (level >= 9) {
    longest_match = FUNCTABLE_FPTR(longest_match_slow);
    insert_string_func = insert_string_roll;
} else {
    longest_match = FUNCTABLE_FPTR(longest_match);
    insert_string_func = insert_string;
}
```

The lazy evaluation logic:
```c
if (s->prev_length >= STD_MIN_MATCH && match_len <= s->prev_length) {
    // Previous match was better — emit it
    bflush = zng_tr_tally_dist(s, s->strstart - 1 - s->prev_match,
                                s->prev_length - STD_MIN_MATCH);
    s->prev_length -= 1;
    s->lookahead -= s->prev_length;
    // Insert strings for the matched region
}
```

#### `deflate_stored.c`

The **stored** strategy (level 0) copies input without compression:

- Emits stored block headers: `BFINAL` flag + 16-bit `LEN` + 16-bit `NLEN`
- Directly copies from `next_in` to `next_out` when possible
- Falls back to copying through the window when direct copy isn't possible
- Tracks hash table state for potential `deflateParams()` level changes

#### `deflate_huff.c`

The **Huffman-only** strategy emits every byte as a literal with no LZ77
matching. Used with `Z_HUFFMAN_ONLY` strategy:

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

#### `deflate_rle.c`

The **RLE** strategy only searches for runs of identical bytes (distance = 1):

- Does not use hash tables
- Compares `scan[0] == scan[1] && scan[1] == scan[2]` to detect a run start
- Uses `compare256_rle()` to find run length
- Emits `zng_tr_tally_dist(s, 1, match_len - STD_MIN_MATCH)`

---

### Core Decompression Engine

#### `inflate.c`

The main decompression module, implementing a large state machine. Key functions:

- **`inflateInit2()`** — Allocates unified buffer via `alloc_inflate()`,
  initialises `inflate_state`, sets window bits and wrap mode.
- **`inflate()`** — Main decompression entry point. A massive `switch` over
  `inflate_mode` enum values.
- **`inflateEnd()`** — Frees resources via `free_inflate()`.
- **`inflateReset()`** — Resets state for reuse.
- **`inflateSetDictionary()`** — Loads a preset dictionary.
- **`inflateSync()`** — Searches for a valid sync point in corrupted data.
- **`inflateCopy()`** — Deep-copies an inflate stream.
- **`inflateMark()`** — Reports decompression progress.
- **`updatewindow()`** — Copies decompressed data to the sliding window
  for back-reference support.

#### `inflate.h`

Defines the `inflate_state` structure and the `inflate_mode` enum.

The inflate state machine has these mode categories:

1. **Header processing**: `HEAD → FLAGS → TIME → OS → EXLEN → EXTRA → NAME → COMMENT → HCRC → TYPE` (gzip) or `HEAD → DICTID/TYPE` (zlib) or `HEAD → TYPEDO` (raw)
2. **Block reading**: `TYPE → TYPEDO → STORED/TABLE/LEN_/CHECK`
3. **Data decoding**: `LEN → LENEXT → DIST → DISTEXT → MATCH`, `LIT → LEN`
4. **Trailer**: `CHECK → LENGTH → DONE`

```c
struct ALIGNED_(64) inflate_state {
    PREFIX3(stream) *strm;
    inflate_mode mode;
    int last;                    // Processing last block?
    int wrap;                    // zlib/gzip/raw mode
    int havedict;
    int flags;                   // gzip header flags
    unsigned long check;         // Running checksum
    unsigned long total;         // Running output count

    // Sliding window
    unsigned wbits;              // log2(window size)
    uint32_t wsize;
    uint32_t whave;              // Valid bytes in window
    uint32_t wnext;              // Write index
    unsigned char *window;

    // Bit accumulator
    uint64_t hold;               // 64-bit input bit accumulator
    unsigned bits;               // Bits in hold

    // Code tables
    unsigned lenbits;            // Index bits for length codes
    code const *lencode;         // Length/literal code table
    code const *distcode;        // Distance code table
    unsigned distbits;           // Index bits for distance codes

    // Dynamic table building
    unsigned ncode, nlen, ndist;
    uint32_t have;
    code *next;

    uint16_t lens[320];          // Code lengths
    uint16_t work[288];          // Work area
    code codes[ENOUGH];          // Code tables (ENOUGH = 1924)
};
```

#### `inflate_p.h`

Private inflate helpers including checksum computation wrappers:

```c
static inline void inf_chksum_cpy(PREFIX3(stream) *strm, uint8_t *dst,
                                   const uint8_t *src, uint32_t copy) {
    struct inflate_state *state = (struct inflate_state*)strm->state;
    if (state->flags)  // gzip mode
        strm->adler = state->check = FUNCTABLE_CALL(crc32_copy)(...);
    else               // zlib mode
        strm->adler = state->check = FUNCTABLE_CALL(adler32_copy)(...);
}
```

#### `inftrees.c` / `inftrees.h`

Builds Huffman decoding tables for inflate. The `code` structure:

```c
typedef struct {
    unsigned char bits;   // Bits in this code part
    unsigned char op;     // Operation: literal, table link, length/distance, EOB, invalid
    uint16_t val;         // Value or table offset
} code;
```

`zng_inflate_table()` constructs a two-level table from code lengths:

```c
int zng_inflate_table(codetype type, uint16_t *lens, unsigned codes,
                      code **table, unsigned *bits, uint16_t *work);
```

The `ENOUGH` constant (1924) is the proven maximum table size:
- `ENOUGH_LENS = 1332` for literal/length codes (286 symbols, root 10 bits, max 15 bits)
- `ENOUGH_DISTS = 592` for distance codes (30 symbols, root 9 bits, max 15 bits)

#### `inffast_tpl.h`

Template for the fast inflate inner loop. Processes literal/length codes
without returning to the main `inflate()` state machine loop, significantly
reducing overhead for long runs of data. Architecture-specific versions
(`inflate_fast_sse2`, `inflate_fast_avx2`, etc.) instantiate this template
with SIMD chunk copying.

#### `infback.c`

An alternative inflate interface where the caller provides input/output
callbacks instead of managing buffers directly. Used by specialised
applications that need fine-grained control over I/O.

---

### Huffman Tree Management

#### `trees.c`

Constructs and emits Huffman trees for deflate output. Key functions:

- **`zng_tr_init()`** — Initialises tree descriptors
- **`init_block()`** — Zeroes frequency counts for a new block
- **`build_tree()`** — Constructs a Huffman tree from frequency data
- **`gen_bitlen()`** — Generates optimal bit lengths
- **`scan_tree()`** — Scans tree for repeat counts (for code length encoding)
- **`send_tree()`** — Emits encoded tree structure
- **`build_bl_tree()`** — Builds the bit-length tree for encoding the main trees
- **`send_all_trees()`** — Emits all three trees (literal, distance, bit-length)
- **`compress_block()`** — Emits compressed data using the specified trees
- **`detect_data_type()`** — Heuristic for binary vs. text classification
- **`zng_tr_flush_block()`** — Decides between stored, static, or dynamic blocks
- **`zng_tr_align()`** — Pads to byte boundary between blocks

Three types of deflate blocks:
1. **Stored** (`STORED_BLOCK = 0`) — No compression
2. **Static trees** (`STATIC_TREES = 1`) — Fixed, predefined Huffman codes
3. **Dynamic trees** (`DYN_TREES = 2`) — Custom trees optimised for the data

#### `trees.h`

Declares tree-related constants and function prototypes:

```c
#define DIST_CODE_LEN 512
#define MAX_BL_BITS 7
```

#### `trees_emit.h`

Inline functions and macros for bit-level output:

- **`send_bits()`** — Packs bits into the 64-bit `bi_buf`, flushing when full
- **`send_code()`** — Emits a Huffman code using `send_bits()`
- **`bi_windup()`** — Flushes remaining bits and aligns to byte boundary
- **`zng_tr_emit_tree()`** — Emits block type marker
- **`zng_tr_emit_end_block()`** — Emits end-of-block code
- **`zng_tr_emit_lit()`** — Emits a literal using tree lookup
- **`zng_tr_emit_dist()`** — Emits a length/distance pair

The 64-bit bit buffer:
```c
#define send_bits(s, t_val, t_len, bi_buf, bi_valid) {   \
    uint64_t val = (uint64_t)t_val;                       \
    uint32_t len = (uint32_t)t_len;                       \
    uint32_t total_bits = bi_valid + len;                  \
    if (total_bits < BIT_BUF_SIZE && bi_valid < BIT_BUF_SIZE) { \
        bi_buf |= val << bi_valid;                        \
        bi_valid = total_bits;                             \
    } else {                                               \
        /* flush and continue */                           \
    }                                                      \
}
```

#### `trees_tbl.h`

Precomputed static Huffman tree tables:
- `static_ltree[L_CODES+2]` — Static literal/length tree values
- `static_dtree[D_CODES]` — Static distance tree values
- `zng_dist_code[DIST_CODE_LEN]` — Distance to distance-code mapping
- `zng_length_code[STD_MAX_MATCH-STD_MIN_MATCH+1]` — Length to length-code mapping
- `extra_lbits[]`, `extra_dbits[]`, `extra_blbits[]` — Extra bits per code
- `lbase_extra[]`, `dbase_extra[]` — Combined base+extra tables for single-lookup

---

### Hash Table and Match Finding

#### `insert_string.c`

Implements hash table insert operations for deflate. Two variants:
- **`insert_string()`** — Standard insert for levels 1–8
- **`insert_string_roll()`** — Rolling insert for level 9

#### `insert_string_tpl.h`

Template for hash table operations:

```c
Z_FORCEINLINE static uint32_t UPDATE_HASH(uint32_t h, uint32_t val) {
    HASH_CALC(h, val);
    return h & HASH_CALC_MASK;
}

Z_FORCEINLINE static uint32_t QUICK_INSERT_VALUE(deflate_state *const s,
                                                  uint32_t str, uint32_t val) {
    // Compute hash, insert into head[], update prev[]
    hm = HASH_CALC_VAR & HASH_CALC_MASK;
    head = s->head[hm];
    if (LIKELY(head != str)) {
        s->prev[str & W_MASK(s)] = (Pos)head;
        s->head[hm] = (Pos)str;
    }
    return head;
}
```

#### `insert_string_p.h`

Private header that includes `insert_string_tpl.h` and defines the hash
function. The hash is computed from 4 bytes read at the current position.

#### `match_tpl.h`

Template for the `longest_match()` function family. Two variants are
generated:
- **`longest_match()`** — Standard match for levels 1–8
- **`longest_match_slow()`** — Enhanced match for level 9 with re-rooting
  of hash chains

The match finding algorithm:
1. Read 8 bytes at scan start and scan+offset for quick rejection
2. Walk the hash chain (`prev[]` array)
3. For each candidate, compare first/last bytes for early rejection
4. Call `compare256()` to find actual match length
5. Update best match if this one is longer
6. Stop when chain length exhausted or `nice_match` reached

```c
Z_INTERNAL uint32_t LONGEST_MATCH(deflate_state *const s, uint32_t cur_match) {
    // ...
    while (--chain_length) {
        match = mbase_start + cur_match;
        // Quick rejection: compare last bytes
        if (zng_memread_8(match+offset) == scan_end &&
            zng_memread_8(match) == scan_start) {
            // Full comparison
            len = compare256(scan+2, match+2) + 2;
            if (len > best_len) {
                s->match_start = cur_match;
                best_len = len;
                if (best_len >= nice_match) return best_len;
                // Update scan_end for new best
            }
        }
        cur_match = prev[cur_match & wmask];
    }
}
```

---

### Checksum Implementations

#### `adler32.c`

Entry point for Adler-32 checksum computation. Dispatches to
`FUNCTABLE_CALL(adler32)`:

```c
uint32_t Z_EXPORT PREFIX(adler32)(uint32_t adler, const unsigned char *buf, uint32_t len) {
    if (buf == NULL) return ADLER32_INITIAL_VALUE;
    return FUNCTABLE_CALL(adler32)(adler, buf, len);
}
```

Also provides `adler32_combine()` for concatenating checksums without
re-reading data.

#### `adler32_p.h`

Scalar Adler-32 implementation using unrolled loops:

```c
#define BASE 65521U     // Largest prime < 65536
#define NMAX 5552       // Largest n such that 255n(n+1)/2 + (n+1)(BASE-1) ≤ 2^32-1

#define ADLER_DO1(sum1, sum2, buf, i)  {(sum1) += buf[(i)]; (sum2) += (sum1);}
#define ADLER_DO16(sum1, sum2, buf)    {ADLER_DO8(sum1,sum2,buf,0); ADLER_DO8(sum1,sum2,buf,8);}
```

The Adler-32 value has two 16-bit halves:
- `sum1` (lower 16 bits) = running sum of bytes mod `BASE`
- `sum2` (upper 16 bits) = running sum of `sum1` values mod `BASE`

#### `crc32.c`

Entry point for CRC-32 computation. Dispatches to `FUNCTABLE_CALL(crc32)`:

```c
uint32_t Z_EXPORT PREFIX(crc32)(uint32_t crc, const unsigned char *buf, uint32_t len) {
    if (buf == NULL) return CRC32_INITIAL_VALUE;
    return FUNCTABLE_CALL(crc32)(crc, buf, len);
}
```

#### `crc32_braid_p.h`

Configures the **braided** CRC algorithm:

```c
#define BRAID_N 5       // Number of braids (interleaved CRC computations)
#define BRAID_W 8       // Word width (8 bytes on 64-bit, 4 on 32-bit)
#define POLY 0xedb88320 // CRC polynomial (reflected)
```

#### `crc32_braid_comb.c`

Implements CRC-32 combination for `crc32_combine()`, allowing merging of
checksums from independently-processed data segments.

#### `crc32_chorba_p.h`

The **Chorba** CRC-32 algorithm — a modern approach using pipeline-friendly
interleaved computation, contributed by Kadatch & Jenkins (2010).

---

### Runtime Dispatch

#### `cpu_features.c` / `cpu_features.h`

Portable CPU feature detection. `cpu_check_features()` fills a
`struct cpu_features` by calling the appropriate architecture-specific
detector:

```c
void cpu_check_features(struct cpu_features *features) {
    memset(features, 0, sizeof(struct cpu_features));
#if defined(X86_FEATURES)
    x86_check_features(&features->x86);
#elif defined(ARM_FEATURES)
    arm_check_features(&features->arm);
    // ... etc.
}
```

The `cpu_features` union:
```c
struct cpu_features {
#if defined(X86_FEATURES)
    struct x86_cpu_features x86;
#elif defined(ARM_FEATURES)
    struct arm_cpu_features arm;
    // ...
};
```

#### `functable.c` / `functable.h`

The function dispatch table. All performance-critical functions are called
through `functable`:

```c
struct functable_s {
    int      (* force_init)         (void);
    uint32_t (* adler32)            (uint32_t, const uint8_t*, size_t);
    uint32_t (* adler32_copy)       (uint32_t, uint8_t*, const uint8_t*, size_t);
    uint8_t* (* chunkmemset_safe)   (uint8_t*, uint8_t*, size_t, size_t);
    uint32_t (* compare256)         (const uint8_t*, const uint8_t*);
    uint32_t (* crc32)              (uint32_t, const uint8_t*, size_t);
    uint32_t (* crc32_copy)         (uint32_t, uint8_t*, const uint8_t*, size_t);
    void     (* inflate_fast)       (PREFIX3(stream)*, uint32_t);
    uint32_t (* longest_match)      (deflate_state*, uint32_t);
    uint32_t (* longest_match_slow) (deflate_state*, uint32_t);
    void     (* slide_hash)         (deflate_state*);
};
```

Initialisation uses a cascading priority system: start with generic C
fallbacks, then override with progressively better SIMD implementations
based on detected features:

```
Generic C  →  SSE2  →  SSSE3  →  SSE4.2  →  AVX2  →  AVX-512  →  VNNI
Generic C  →  NEON  →  CRC32  →  PMULL+EOR3
Generic C  →  VMX   →  POWER8  →  POWER9
```

Thread safety is ensured by atomic pointer stores and memory barriers.

When `DISABLE_RUNTIME_CPU_DETECTION` is defined, all dispatch is resolved
at compile time via `native_` prefixed macros.

---

### Build System Infrastructure

#### `zbuild.h`

Central build-system header. Defines:
- POSIX feature test macros
- Compiler attribute wrappers (`Z_TARGET`, `Z_FORCEINLINE`, `Z_FALLTHROUGH`)
- `ssize_t` definition for MSVC
- Platform-specific `Z_EXPORT` / `Z_INTERNAL` visibility macros
- `ALIGNED_()` macro for struct alignment

#### `zutil.h`

Internal utility header. Defines:
- `STD_MIN_MATCH = 3`, `STD_MAX_MATCH = 258`
- `WANT_MIN_MATCH = 4` (internal performance optimisation)
- `DEF_WBITS = MAX_WBITS`
- Block type constants: `STORED_BLOCK = 0`, `STATIC_TREES = 1`, `DYN_TREES = 2`
- Error messages array `z_errmsg[]`
- Initial checksum values: `ADLER32_INITIAL_VALUE = 1`, `CRC32_INITIAL_VALUE = 0`
- Wrapper overhead: `ZLIB_WRAPLEN = 6`, `GZIP_WRAPLEN = 18`
- Deflate block overhead constants

#### `zutil.c`

Implements `z_errmsg[]`, default `zcalloc()` / `zcfree()` allocators, and
`zlibCompileFlags()`.

#### `zendian.h`

Endianness detection and byte-swap macros for little-endian / big-endian architectures.

#### `zmemory.h`

Provides portable aligned memory read/write functions: `zng_memread_2()`,
`zng_memread_4()`, `zng_memread_8()`, `zng_memwrite_2()`, `zng_memwrite_4()`.

#### `zarch.h`

Architecture detection macros. Identifies the target CPU family and word
size (`ARCH_32BIT` / `ARCH_64BIT`), optimal comparison width (`OPTIMAL_CMP`).

#### `arch_functions.h`

Includes the appropriate `arch/<platform>/<platform>_functions.h` header
to declare architecture-specific function variants.

#### `arch_natives.h`

Includes the appropriate `arch/<platform>/<platform>_natives.h` header
to define `native_` macros for compile-time dispatch.

---

### Architecture-Specific Implementations

Each directory under `arch/` follows the same pattern:

```
arch/<platform>/
├── <platform>_features.c / .h   # CPU feature detection
├── <platform>_functions.h       # Function declarations
├── <platform>_natives.h         # Compile-time dispatch macros
├── adler32_*.c                  # SIMD Adler-32
├── crc32_*.c                    # SIMD CRC-32
├── chunkset_*.c                 # SIMD memory set (for inflate)
├── compare256_*.c               # SIMD 256-byte comparison (for match finding)
├── slide_hash_*.c               # SIMD hash table sliding
└── Makefile.in                  # Makefile fragment
```

#### `arch/generic/`

Portable C fallback implementations:
- `adler32_c.c` — Scalar Adler-32
- `crc32_braid_c.c` — Braided CRC-32
- `crc32_chorba_c.c` — Chorba CRC-32 (generic)
- `compare256_c.c` — Byte-by-byte / word comparison
- `chunkset_c.c` — Scalar chunk copy for inflate
- `slide_hash_c.c` — Scalar hash table slide

These are always compiled and serve as the baseline when no SIMD is available.

#### `arch/x86/`

x86 SIMD implementations spanning SSE2 through AVX-512:
- `adler32_ssse3.c`, `adler32_avx2.c`, `adler32_avx512.c`, `adler32_avx512_vnni.c`, `adler32_sse42.c`
- `crc32_pclmulqdq.c`, `crc32_vpclmulqdq_avx2.c`, `crc32_vpclmulqdq_avx512.c`
- `crc32_chorba_sse2.c`, `crc32_chorba_sse41.c`
- `compare256_sse2.c`, `compare256_avx2.c`, `compare256_avx512.c`
- `chunkset_sse2.c`, `chunkset_ssse3.c`, `chunkset_avx2.c`, `chunkset_avx512.c`
- `slide_hash_sse2.c`, `slide_hash_avx2.c`
- `x86_features.c` — CPUID-based feature detection

#### `arch/arm/`

ARM SIMD implementations:
- `adler32_neon.c` — NEON Adler-32
- `crc32_armv8.c` — Hardware CRC-32 instructions
- `crc32_armv8_pmull_eor3.c` — PMULL polynomial multiply with EOR3
- `compare256_neon.c` — NEON comparison
- `chunkset_neon.c` — NEON chunk copy
- `slide_hash_neon.c` — NEON hash slide
- `slide_hash_armv6.c` — ARMv6 SIMD hash slide
- `arm_features.c` — Runtime feature detection

---

## Data Flow

### Compression Data Flow

```
User calls deflate(strm, flush)
    │
    ├─ Header emission (if status == INIT_STATE or GZIP_STATE)
    │   └─ zlib: CMF + FLG bytes
    │   └─ gzip: ID1+ID2+CM+FLG+MTIME+XFL+OS + optional fields
    │
    ├─ Strategy function call (e.g., deflate_slow)
    │   │
    │   ├─ fill_window(): read from next_in into window[]
    │   │   └─ slide_hash() when window fills (FUNCTABLE dispatch)
    │   │
    │   ├─ Hash insert: insert_string / quick_insert_string
    │   │   └─ head[hash] = position; prev[position] = old_head
    │   │
    │   ├─ Match finding: longest_match() (FUNCTABLE dispatch)
    │   │   └─ Walk prev[] chain, call compare256() (FUNCTABLE dispatch)
    │   │
    │   ├─ Tally: zng_tr_tally_lit() or zng_tr_tally_dist()
    │   │   └─ Store in sym_buf (or d_buf/l_buf)
    │   │   └─ Update dyn_ltree[].Freq, dyn_dtree[].Freq
    │   │
    │   └─ Block flush: zng_tr_flush_block()
    │       ├─ build_tree() for literal, distance, bit-length trees
    │       ├─ Compare opt_len (dynamic) vs static_len vs stored size
    │       ├─ send_all_trees() + compress_block() (dynamic)
    │       │   or compress_block(static_ltree, static_dtree)
    │       │   or stored block
    │       └─ Output through pending_buf → next_out
    │
    ├─ Checksum update: strm->adler via adler32() or crc32()
    │
    └─ Trailer emission (if flush == Z_FINISH)
        └─ zlib: 4-byte Adler-32
        └─ gzip: 4-byte CRC-32 + 4-byte ISIZE
```

### Decompression Data Flow

```
User calls inflate(strm, flush)
    │
    ├─ Header parsing (HEAD → TYPE modes)
    │   └─ zlib: check CMF/FLG, read FDICT/Adler
    │   └─ gzip: check magic, parse FLG bits, skip extra/name/comment
    │
    ├─ Block processing (TYPE → data modes)
    │   │
    │   ├─ STORED: read LEN/NLEN, copy raw bytes
    │   │
    │   ├─ TABLE → LENLENS → CODELENS:
    │   │   └─ Read code-length code lengths
    │   │   └─ inflate_table(CODES, ...) → decode code lengths
    │   │   └─ inflate_table(LENS, ...) → build lencode table
    │   │   └─ inflate_table(DISTS, ...) → build distcode table
    │   │
    │   └─ LEN → LENEXT → DIST → DISTEXT → MATCH:
    │       ├─ Decode literal/length from lencode table
    │       ├─ If literal: output byte
    │       ├─ If length: read extra bits
    │       │   └─ Decode distance from distcode table + extra bits
    │       │   └─ Copy from window: chunkmemset_safe() (FUNCTABLE)
    │       └─ inflate_fast() for hot inner loop (FUNCTABLE dispatch)
    │
    ├─ Checksum verification: inf_chksum() / inf_chksum_cpy()
    │   └─ crc32_copy() or adler32_copy() (FUNCTABLE dispatch)
    │
    └─ Trailer verification (CHECK → DONE)
        └─ Compare computed checksum with stored value
```

---

## Cache-Line Optimisation

The `deflate_state` structure is explicitly partitioned into cache lines
(marked with comments in the source):

- **Cacheline 0** (bytes 0–63): Stream pointer, pending buffer, status — accessed
  on every `deflate()` call
- **Cacheline 1** (bytes 64–127): Window pointers, lookahead, hash state — accessed
  during match finding
- **Cacheline 2** (bytes 128–191): Match parameters, compression level — accessed
  during strategy decisions
- **Cacheline 3** (bytes 192–255): Padding for tree data alignment

The Huffman trees (`dyn_ltree`, `dyn_dtree`, `bl_tree`) follow in subsequent
cache lines, accessed only during tree construction and block flushing.

---

## Conditional Compilation

Key preprocessor macros controlling behaviour:

| Macro | Effect |
|---|---|
| `ZLIB_COMPAT` | Enable zlib-compatible API (rename all symbols) |
| `WITH_GZFILEOP` | Include gzip file I/O functions |
| `NO_GZIP` | Disable gzip header support (define `GZIP` / `GUNZIP`) |
| `LIT_MEM` | Use separate distance/length buffers |
| `NO_LIT_MEM` | Force overlaid `sym_buf` |
| `NO_QUICK_STRATEGY` | Disable `deflate_quick`, use `deflate_fast` for level 1 |
| `NO_MEDIUM_STRATEGY` | Disable `deflate_medium`, use `deflate_fast`/`deflate_slow` |
| `DISABLE_RUNTIME_CPU_DETECTION` | Compile-time function selection only |
| `WITH_ALL_FALLBACKS` | Build all generic fallbacks (useful for benchmarking) |
| `WITH_REDUCED_MEM` | Smaller buffers for memory-constrained environments |
| `INFLATE_STRICT` | Strict distance checking in inflate |
| `INFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR` | Zero-fill invalid distances |
| `ZLIB_DEBUG` | Enable debug assertions and tracing |
| `WITHOUT_CHORBA` | Disable Chorba CRC-32 algorithm |
