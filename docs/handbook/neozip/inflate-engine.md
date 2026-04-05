# Inflate Engine

## Overview

The inflate engine decompresses DEFLATE-format data (RFC 1951) wrapped in
either a zlib container (RFC 1950), a gzip container (RFC 1952), or with no
wrapper (raw deflate). It is implemented as a state machine in `inflate.c`,
with a hot inner loop in `inffast_tpl.h` and table-building in `inftrees.c`.

The engine processes input byte-by-byte through a 64-bit bit accumulator
(`hold`), transitioning between states as it parses headers, decodes Huffman
codes, copies back-referenced data, and verifies integrity checksums.

---

## State Machine

The `inflate_mode` enum defines all possible states:

```c
typedef enum {
    HEAD = 16180,   // Waiting for magic header
    FLAGS,          // Waiting for method and flags (gzip)
    TIME,           // Waiting for modification time (gzip)
    OS,             // Waiting for extra flags and OS (gzip)
    EXLEN,          // Waiting for extra length (gzip)
    EXTRA,          // Waiting for extra bytes (gzip)
    NAME,           // Waiting for end of file name (gzip)
    COMMENT,        // Waiting for end of comment (gzip)
    HCRC,           // Waiting for header CRC (gzip)
    DICTID,         // Waiting for dictionary check value
    DICT,           // Waiting for inflateSetDictionary() call
    TYPE,           // Waiting for type bits (including last-flag bit)
    TYPEDO,         // Same as TYPE but skip check to exit on new block
    STORED,         // Waiting for stored size (length and complement)
    COPY_,          // Waiting for input or output to copy stored block (first time)
    COPY,           // Waiting for input or output to copy stored block
    TABLE,          // Waiting for dynamic block table lengths
    LENLENS,        // Waiting for code length code lengths
    CODELENS,       // Waiting for length/lit and distance code lengths
    LEN_,           // Waiting for length/lit/eob code (first time)
    LEN,            // Waiting for length/lit/eob code
    LENEXT,         // Waiting for length extra bits
    DIST,           // Waiting for distance code
    DISTEXT,        // Waiting for distance extra bits
    MATCH,          // Waiting for output space to copy string
    LIT,            // Waiting for output space to write literal
    CHECK,          // Waiting for 32-bit check value
    LENGTH,         // Waiting for 32-bit length (gzip)
    DONE,           // Finished check, remain here until reset
    BAD,            // Got a data error, remain here until reset
    SYNC            // Looking for synchronization bytes
} inflate_mode;
```

The starting value `HEAD = 16180` (a non-zero, distinctive value) helps catch
uninitialised state errors.

---

## State Transitions

### Header Processing

```
HEAD ─┬─ (gzip header detected) ──▶ FLAGS ─▶ TIME ─▶ OS ─▶ EXLEN ─▶ EXTRA
      │                              ─▶ NAME ─▶ COMMENT ─▶ HCRC ─▶ TYPE
      │
      ├─ (zlib header detected) ──▶ DICTID ─▶ DICT ─▶ TYPE
      │                             or
      │                            ──▶ TYPE (no dictionary)
      │
      └─ (raw deflate) ──────────▶ TYPEDO
```

### Block Processing

```
TYPE ──▶ TYPEDO ─┬─ (stored block)  ──▶ STORED ─▶ COPY_ ─▶ COPY ──▶ TYPE
                 │
                 ├─ (dynamic block) ──▶ TABLE ─▶ LENLENS ─▶ CODELENS ─▶ LEN_
                 │
                 ├─ (fixed block)   ──▶ LEN_
                 │
                 └─ (last block)    ──▶ CHECK
```

### Data Decoding

```
LEN_ ──▶ LEN ─┬─ (literal)        ──▶ LIT ──▶ LEN
               │
               ├─ (length code)    ──▶ LENEXT ──▶ DIST ──▶ DISTEXT ──▶ MATCH ──▶ LEN
               │
               └─ (end-of-block)   ──▶ TYPE (or CHECK if last block)
```

### Trailer Processing

```
CHECK ──▶ LENGTH (gzip only) ──▶ DONE
```

---

## The `inflate_state` Structure

```c
struct ALIGNED_(64) inflate_state {
    // Stream back-pointer
    PREFIX3(stream) *strm;

    // State machine
    inflate_mode mode;      // Current state
    int last;               // true if processing last block
    int wrap;               // bit 0: zlib, bit 1: gzip, bit 2: validate check
    int havedict;           // Dictionary provided?
    int flags;              // gzip header flags (-1 = no header yet, 0 = zlib)

    // Integrity
    unsigned was;           // Initial match length for inflateMark
    unsigned long check;    // Running checksum (Adler-32 or CRC-32)
    unsigned long total;    // Running output byte count
    PREFIX(gz_headerp) head;// Where to save gzip header info

    // Sliding window
    unsigned wbits;         // log2(requested window size)
    uint32_t wsize;         // Window size (or 0 if not allocated)
    uint32_t wbufsize;      // Real allocated window size including padding
    uint32_t whave;         // Valid bytes in window
    uint32_t wnext;         // Window write index
    unsigned char *window;  // Sliding window buffer

    // Bit accumulator
    uint64_t hold;          // 64-bit input bit accumulator
    unsigned bits;          // Bits currently held

    // Code tables
    unsigned lenbits;       // Root bits for length code table
    code const *lencode;    // Length/literal code table pointer
    code const *distcode;   // Distance code table pointer
    unsigned distbits;      // Root bits for distance code table

    // Current match state
    uint32_t length;        // Literal value or copy length
    unsigned offset;        // Copy distance
    unsigned extra;         // Extra bits to read

    // Dynamic table building
    unsigned ncode;         // Code length code lengths count
    unsigned nlen;          // Length code count
    unsigned ndist;         // Distance code count
    uint32_t have;          // Code lengths decoded so far
    code *next;             // Next free slot in codes[]

    // Working storage
    uint16_t lens[320];     // Code lengths (max 286 + 30 + padding)
    uint16_t work[288];     // Work area for inflate_table
    code codes[ENOUGH];     // Code tables (ENOUGH = 1924 entries)

    inflate_allocs *alloc_bufs;
};
```

### Key Dimensions

| Constant | Value | Purpose |
|---|---|---|
| `ENOUGH` | 1924 | Maximum total code table entries |
| `ENOUGH_LENS` | 1332 | Maximum literal/length table entries |
| `ENOUGH_DISTS` | 592 | Maximum distance table entries |
| `MAX_WBITS` | 15 | Maximum window bits (32KB window) |
| `MIN_WBITS` | 8 | Minimum window bits (256-byte window) |

---

## Bit Accumulator

The inflate engine uses a 64-bit bit buffer for efficient bit extraction:

```c
uint64_t hold;    // Accumulated bits (LSB first)
unsigned bits;    // Number of valid bits in hold
```

Bits are read from `hold` from the least significant end:

```c
// Load bits from input
#define PULLBYTE() do { \
    hold += (uint64_t)(*next++) << bits; \
    bits += 8; \
} while (0)

// Ensure at least n bits are available
#define NEEDBITS(n) do { \
    while (bits < (unsigned)(n)) \
        PULLBYTE(); \
} while (0)

// Read n bits from hold
#define BITS(n) ((unsigned)hold & ((1U << (n)) - 1))

// Drop n bits from hold
#define DROPBITS(n) do { \
    hold >>= (n); \
    bits -= (unsigned)(n); \
} while (0)
```

The 64-bit width allows accumulating up to 8 bytes before overflow, reducing
the frequency of input reads.

---

## Huffman Code Decoding

### The `code` Structure

Each entry in a decoding table is a `code` structure:

```c
typedef struct {
    unsigned char bits;   // Bits to consume from input
    unsigned char op;     // Operation type + extra bits
    uint16_t val;         // Output value or table offset
} code;
```

The `op` field encodes the meaning:

| `op` Value | Meaning |
|---|---|
| `0x00` | Literal: `val` is the literal byte |
| `0x0t` (t ≠ 0) | Table link: `t` is the number of additional index bits |
| `0x1e` | Length or distance: `e` extra bits, `val` is the base value |
| `0x60` | End of block |
| `0x40` | Invalid code |

### Table Building via `zng_inflate_table()`

`inftrees.c` builds two-level Huffman decoding tables:

```c
int zng_inflate_table(codetype type, uint16_t *lens, unsigned codes,
                      code **table, unsigned *bits, uint16_t *work);
```

Parameters:
- `type` — `CODES` (code lengths), `LENS` (literal/length), or `DISTS` (distances)
- `lens` — Array of code lengths for each symbol
- `codes` — Number of symbols
- `table` — Output: pointer to the root table
- `bits` — Input: requested root table bits; Output: actual root table bits
- `work` — Scratch space

The algorithm:
1. Count code lengths → `count[]`
2. Compute offsets for sorting → `offs[]`
3. Sort symbols by code length → `work[]`
4. Fill the primary table (root bits = `*bits`)
5. For codes longer than root bits, create sub-tables

Root table sizes:
- Literal/length: 10 bits (1024 entries)
- Distance: 9 bits (512 entries)

Sub-tables handle codes longer than the root bits, linked from the primary
table via the `op` field.

### Fixed Huffman Tables

For fixed-code blocks (BTYPE=01), predefined tables are stored in
`inffixed_tbl.h`. These are computed once and reused.

The `fixedtables()` function in `inflate.c` sets up the fixed tables:
```c
state->lencode = lenfix;     // Predefined literal/length table
state->lenbits = 9;          // Root bits for fixed literal/length codes
state->distcode = distfix;   // Predefined distance table
state->distbits = 5;         // Root bits for fixed distance codes
```

---

## The Inflate Loop

The main `inflate()` function is a single large `for` loop with a `switch`
on `state->mode`:

```c
int32_t Z_EXPORT PREFIX(inflate)(PREFIX3(stream) *strm, int32_t flush) {
    // Input/output tracking
    unsigned char *put = strm->next_out;
    unsigned char *next = strm->next_in;
    uint64_t hold = state->hold;
    unsigned bits = state->bits;

    for (;;) switch (state->mode) {
    case HEAD:
        // Detect zlib/gzip/raw header
        NEEDBITS(16);
        if (state->wrap == 0) {
            // Raw deflate
            state->mode = TYPEDO;
            break;
        }
        if ((hold & 0xff) == 0x1f && ((hold >> 8) & 0xff) == 0x8b) {
            // gzip header
            state->mode = FLAGS;
        } else {
            // zlib header (CMF + FLG)
            state->mode = DICTID; // or TYPE
        }
        break;

    case TYPE:
        // Read block header
        NEEDBITS(3);
        state->last = BITS(1);
        DROPBITS(1);
        switch (BITS(2)) {
        case 0: state->mode = STORED; break;  // Stored block
        case 1: fixedtables(state);            // Fixed Huffman
                state->mode = LEN_; break;
        case 2: state->mode = TABLE; break;    // Dynamic Huffman
        case 3: state->mode = BAD; break;      // Reserved (error)
        }
        DROPBITS(2);
        break;

    case LEN:
        // Decode literal/length code
        here = state->lencode[BITS(state->lenbits)];
        if (here.op == 0) {
            // Literal
            *put++ = (unsigned char)(here.val);
            DROPBITS(here.bits);
            state->mode = LEN;
        } else if (here.op & 16) {
            // Length code
            state->length = here.val;
            state->extra = here.op & 15;
            state->mode = LENEXT;
        } else if (here.op == 96) {
            // End of block
            state->mode = TYPE;
        }
        break;

    case DIST:
        // Decode distance code
        here = state->distcode[BITS(state->distbits)];
        // ... similar pattern ...
        break;

    case MATCH:
        // Copy from window
        // Copy state->length bytes from position (out - state->offset)
        // Uses chunkmemset_safe() for SIMD-accelerated copying
        break;

    case CHECK:
        // Verify checksum
        // Compare computed check with stored value
        break;

    case DONE:
        ret = Z_STREAM_END;
        goto inf_leave;
    }

inf_leave:
    // Save state
    state->hold = hold;
    state->bits = bits;
    return ret;
}
```

---

## Fast Inflate Path

When sufficient input and output are available, `inflate()` calls the fast
inner loop:

```c
case LEN_:
    // Check if we can use the fast path
    if (have >= 6 && left >= 258) {
        FUNCTABLE_CALL(inflate_fast)(strm, out);
        // Reload local state
        state->mode = LEN;
        break;
    }
    state->mode = LEN;
    break;
```

`inflate_fast()` (defined via `inffast_tpl.h`) processes codes without
returning to the main switch loop:

1. Pre-load bits into the 64-bit accumulator
2. Loop while input ≥ 6 bytes and output ≥ 258 bytes:
   - Decode literal/length from `lencode`
   - If literal: output directly
   - If length: decode distance from `distcode`, copy from window
   - If EOB: exit loop
3. Handle sub-table traversal for codes longer than root bits

SIMD variants (SSE2, AVX2, AVX-512, NEON) accelerate the copy operation
via `chunkmemset_safe()`, which can copy overlapping regions efficiently
using vector loads/stores.

---

## Window Management

The inflate sliding window stores recent output for back-reference copying:

```c
unsigned char *window;   // Circular buffer
uint32_t wsize;          // Window size
uint32_t whave;          // Valid bytes in window
uint32_t wnext;          // Write position (circular)
```

### `updatewindow()`

Called after each inflate pass to copy decompressed output into the window:

```c
static void updatewindow(PREFIX3(stream) *strm, const uint8_t *end,
                          uint32_t len, int32_t cksum) {
    struct inflate_state *state = (struct inflate_state *)strm->state;

    // First time: set up window size
    if (state->wsize == 0) {
        state->wsize = 1U << state->wbits;
        state->wnext = 0;
        state->whave = 0;
    }

    // Copy output to window (handles wraparound)
    if (len >= state->wsize) {
        // Copy last wsize bytes
        memcpy(state->window, end - state->wsize, state->wsize);
        state->wnext = 0;
        state->whave = state->wsize;
    } else {
        // Copy len bytes, wrapping around if necessary
        uint32_t dist = MIN(state->wsize - state->wnext, len);
        memcpy(state->window + state->wnext, end - len, dist);
        len -= dist;
        if (len) {
            memcpy(state->window, end - len, len);
            state->wnext = len;
            state->whave = state->wsize;
        } else {
            state->wnext += dist;
            if (state->wnext == state->wsize) state->wnext = 0;
            if (state->whave < state->wsize) state->whave += dist;
        }
    }
}
```

### Back-Reference Copying

When a length/distance pair is decoded, the engine copies `length` bytes
from position `(output_position - distance)`:

```c
case MATCH:
    // Copy from window
    from = put - state->offset;
    if (state->offset > (put - state->window)) {
        // Source is in the circular window buffer
        // Handle wrap-around via window[]
    }
    // Copy length bytes to put, potentially overlapping
    FUNCTABLE_CALL(chunkmemset_safe)(put, from, state->length, left);
```

The `chunkmemset_safe()` function handles the case where `distance < length`
(overlapping copy), which occurs in runs of repeated patterns. SIMD
implementations can vectorise even overlapping copies by replicating the
pattern.

---

## Checksum Handling

During decompression, a running checksum is computed:

- **zlib format**: Adler-32 of the uncompressed data
- **gzip format**: CRC-32 of the uncompressed data

The selection is based on `state->flags`:
```c
static inline void inf_chksum_cpy(PREFIX3(stream) *strm, uint8_t *dst,
                                   const uint8_t *src, uint32_t copy) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    if (state->flags)
        strm->adler = state->check = FUNCTABLE_CALL(crc32_copy)(state->check, dst, src, copy);
    else
        strm->adler = state->check = FUNCTABLE_CALL(adler32_copy)(state->check, dst, src, copy);
}
```

The `_copy` variants compute the checksum while simultaneously copying data,
avoiding a second pass over the output.

After all blocks are decompressed, the stored checksum is read and compared:

```c
case CHECK:
    NEEDBITS(32);
    if (ZSWAP32((unsigned long)hold) != state->check) {
        strm->msg = "incorrect data check";
        state->mode = BAD;
        break;
    }
    state->mode = LENGTH;  // gzip: also check ISIZE
    break;
```

---

## Error Handling

The inflate engine detects and reports several error conditions:

| Error | Detection | Recovery |
|---|---|---|
| Invalid block type (11) | `TYPE` state | `state->mode = BAD` |
| Invalid stored block length | `STORED` state: `LEN != ~NLEN` | `BAD` |
| Invalid code length repeat | `CODELENS` state | `BAD` |
| Invalid literal/length code | Decoding | `BAD` |
| Invalid distance code | Decoding | `BAD` |
| Distance too far back | `MATCH` state | `BAD` (or zero-fill if `INFLATE_ALLOW_INVALID_DIST`) |
| Checksum mismatch | `CHECK` state | `Z_DATA_ERROR` |
| Header version mismatch | `HEAD` state | `Z_STREAM_ERROR` |

The `inflateSync()` function searches for a sync point (four bytes `00 00 FF FF`)
to recover from data corruption. `inflateMark()` reports the current
decompression position for partial recovery.

---

## Inflate API Functions

### Core Functions

```c
int inflateInit(z_stream *strm);
int inflateInit2(z_stream *strm, int windowBits);
int inflate(z_stream *strm, int flush);
int inflateEnd(z_stream *strm);
```

### Window Bits Parameter

The `windowBits` parameter to `inflateInit2()` controls both the window size
and the stream format:

| windowBits | Format | Window Size |
|---|---|---|
| 8..15 | zlib (auto-detect dictionary) | 2^windowBits |
| -8..-15 | Raw deflate (no wrapper) | 2^|windowBits| |
| 24..31 (8..15 + 16) | gzip only | 2^(windowBits-16) |
| 40..47 (8..15 + 32) | Auto-detect zlib or gzip | 2^(windowBits-32) |

### Reset Functions

```c
int inflateReset(z_stream *strm);      // Full reset
int inflateResetKeep(z_stream *strm);  // Reset but keep window
int inflateReset2(z_stream *strm, int windowBits);  // Reset with new windowBits
```

### Dictionary Support

```c
int inflateSetDictionary(z_stream *strm, const unsigned char *dictionary, unsigned dictLength);
int inflateGetDictionary(z_stream *strm, unsigned char *dictionary, unsigned *dictLength);
```

When the zlib header indicates a preset dictionary (`FDICT` flag), `inflate()`
returns `Z_NEED_DICT`. The application must then call `inflateSetDictionary()`
with the correct dictionary before continuing.

### Sync and Recovery

```c
int inflateSync(z_stream *strm);       // Search for sync point
long inflateMark(z_stream *strm);      // Report progress
int inflatePrime(z_stream *strm, int bits, int value);  // Prime bit buffer
```

### Header Access

```c
int inflateGetHeader(z_stream *strm, gz_header *head);  // Get gzip header info
```

### Copy

```c
int inflateCopy(z_stream *dest, z_stream *source);  // Deep copy
```

---

## Memory Allocation

Inflate uses a single-allocation strategy via `alloc_inflate()`:

```c
inflate_allocs* alloc_inflate(PREFIX3(stream) *strm) {
    int window_size = INFLATE_ADJUST_WINDOW_SIZE((1 << MAX_WBITS) + 64);
    int state_size = sizeof(inflate_state);
    int alloc_size = sizeof(inflate_allocs);

    // Calculate positions with alignment padding
    int window_pos = PAD_WINDOW(0);
    int state_pos = PAD_64(window_pos + window_size);
    int alloc_pos = PAD_16(state_pos + state_size);
    int total_size = PAD_64(alloc_pos + alloc_size + (WINDOW_PAD_SIZE - 1));

    char *buf = strm->zalloc(strm->opaque, 1, total_size);
    // Partition buf into window, state, alloc_bufs
    return alloc_bufs;
}
```

A single `zfree()` call releases everything:
```c
void free_inflate(PREFIX3(stream) *strm) {
    inflate_allocs *alloc_bufs = state->alloc_bufs;
    alloc_bufs->zfree(strm->opaque, alloc_bufs->buf_start);
    strm->state = NULL;
}
```

---

## `infback.c` — Callback-Based Inflate

An alternative inflate interface where the application provides input and
output callbacks:

```c
int inflateBack(z_stream *strm,
                in_func in, void *in_desc,
                out_func out, void *out_desc);
```

The caller provides:
- `in(in_desc, &buf)` — Returns available input and sets `buf`
- `out(out_desc, buf, len)` — Consumes `len` bytes of output from `buf`

This avoids buffer management complexity for applications that can provide
data on demand.
