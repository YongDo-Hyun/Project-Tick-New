# Huffman Coding

## Overview

Huffman coding is the heart of DEFLATE compression. It assigns
variable-length bit codes to symbols: shorter codes for frequent symbols,
longer codes for rare ones. Neozip implements full Huffman tree construction,
code generation, and bitstream emission in `trees.c` and `trees_emit.h`.

---

## Data Structures

### `ct_data` — Code/Tree Node

```c
typedef union ct_data_s {
    struct {
        uint16_t freq;     // Frequency count (during tree building)
        uint16_t code;     // Bit string (after tree building)
    };
    struct {
        uint16_t dad;      // Father node in Huffman tree
        uint16_t len;      // Bit length of the code
    };
} ct_data;
```

The union reuses the same 4 bytes: during tree construction, `freq` and
`dad` are used; after code generation, `code` and `len` replace them.

### `tree_desc` — Tree Descriptor

```c
typedef struct tree_desc_s {
    ct_data           *dyn_tree;    // The dynamic tree being built
    int                max_code;    // Largest code with non-zero frequency
    const static_tree_desc *stat_desc;  // Corresponding static tree description
} tree_desc;
```

Each deflate state maintains three tree descriptors:
```c
struct ALIGNED_(64) internal_state {
    tree_desc     l_desc;    // Literal/length tree descriptor
    tree_desc     d_desc;    // Distance tree descriptor
    tree_desc     bl_desc;   // Bit-length tree descriptor (for encoding the dynamic trees)
    // ...
    ct_data       dyn_ltree[HEAP_SIZE];    // Literal/length tree (2*L_CODES+1 = 573)
    ct_data       dyn_dtree[2*D_CODES+1];  // Distance tree (2*30+1 = 61)
    ct_data       bl_tree[2*BL_CODES+1];   // Bit-length tree (2*19+1 = 39)
};
```

### `static_tree_desc` — Static Tree Description

```c
struct static_tree_desc_s {
    const ct_data    *static_tree;    // Static tree (NULL for bit lengths)
    const int        *extra_bits;     // Extra bits for each code (or NULL)
    int               extra_base;     // First code with extra bits
    int               elems;          // Maximum number of elements in tree
    unsigned int      max_length;     // Maximum code bit length
};
```

Three static descriptors exist:
```c
static const static_tree_desc static_l_desc = {
    static_ltree, extra_lbits, LITERALS + 1, L_CODES, MAX_BITS
};
static const static_tree_desc static_d_desc = {
    static_dtree, extra_dbits, 0, D_CODES, MAX_BITS
};
static const static_tree_desc static_bl_desc = {
    NULL, extra_blbits, 0, BL_CODES, MAX_BL_BITS
};
```

---

## Constants

```c
#define L_CODES     286     // Number of literal/length codes (256 literals + END_BLOCK + 29 lengths)
#define D_CODES     30      // Number of distance codes
#define BL_CODES    19      // Number of bit-length codes
#define HEAP_SIZE   (2*L_CODES + 1)  // = 573
#define MAX_BITS    15      // Maximum Huffman code length
#define MAX_BL_BITS 7       // Maximum bit-length code length
#define END_BLOCK   256     // End of block symbol
#define LITERALS    256     // Number of literal bytes (0..255)
#define LENGTH_CODES 29     // Number of length codes (not counting END_BLOCK)
```

### Extra Bits Tables

Length codes (257–285) carry 0–5 extra bits:
```c
static const int extra_lbits[LENGTH_CODES] = {
    0,0,0,0,0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3, 4,4,4,4, 5,5,5,5, 0
};
```

Distance codes (0–29) carry 0–13 extra bits:
```c
static const int extra_dbits[D_CODES] = {
    0,0,0,0, 1,1, 2,2, 3,3, 4,4, 5,5, 6,6, 7,7, 8,8, 9,9, 10,10, 11,11, 12,12, 13,13
};
```

Bit-length codes (used to encode dynamic tree) carry 0–7 extra bits:
```c
static const int extra_blbits[BL_CODES] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 2,3,7
};
```

---

## Tree Construction

### `build_tree()`

The main tree-building function:

```c
static void build_tree(deflate_state *s, tree_desc *desc) {
    ct_data *tree = desc->dyn_tree;
    const ct_data *stree = desc->stat_desc->static_tree;
    int elems = desc->stat_desc->elems;
    int n, m;
    int max_code = -1;
    int node;

    // Step 1: Initialize the heap with leaf frequencies
    s->heap_len = 0;
    s->heap_max = HEAP_SIZE;

    for (n = 0; n < elems; n++) {
        if (tree[n].freq != 0) {
            s->heap[++(s->heap_len)] = max_code = n;
            s->depth[n] = 0;
        } else {
            tree[n].len = 0;
        }
    }

    // Step 2: Ensure at least two codes exist
    while (s->heap_len < 2) {
        node = s->heap[++(s->heap_len)] =
            (max_code < 2 ? ++max_code : 0);
        tree[node].freq = 1;
        s->depth[node] = 0;
    }
    desc->max_code = max_code;

    // Step 3: Build the Huffman tree using a min-heap
    // Nodes elems..HEAP_SIZE-1 are internal nodes
    for (n = s->heap_len / 2; n >= 1; n--)
        pqdownheap(s, tree, n);

    node = elems;
    do {
        n = s->heap[1];              // Least frequent
        pqremove(s, tree, n);
        m = s->heap[1];              // Next least frequent

        s->heap[--(s->heap_max)] = n;
        s->heap[--(s->heap_max)] = m;

        // Create internal node
        tree[node].freq = tree[n].freq + tree[m].freq;
        s->depth[node] = MAX(s->depth[n], s->depth[m]) + 1;
        tree[n].dad = tree[m].dad = (uint16_t)node;

        s->heap[1] = node++;
        pqdownheap(s, tree, 1);
    } while (s->heap_len >= 2);

    s->heap[--(s->heap_max)] = s->heap[1];

    // Step 4: Compute code lengths and generate codes
    gen_bitlen(s, desc);
    gen_codes(tree, max_code, s->bl_count);
}
```

### `pqdownheap()` — Min-Heap Maintenance

```c
static void pqdownheap(deflate_state *s, ct_data *tree, int k) {
    int v = s->heap[k];
    int j = k << 1;  // Left child

    while (j <= s->heap_len) {
        // Select smaller child
        if (j < s->heap_len &&
            SMALLER(tree, s->heap[j+1], s->heap[j], s->depth))
            j++;
        // If v is smaller than both children, stop
        if (SMALLER(tree, v, s->heap[j], s->depth))
            break;
        s->heap[k] = s->heap[j];
        k = j;
        j <<= 1;
    }
    s->heap[k] = v;
}
```

The `SMALLER` macro compares by frequency first, then by depth:
```c
#define SMALLER(tree, n, m, depth) \
    (tree[n].freq < tree[m].freq || \
     (tree[n].freq == tree[m].freq && depth[n] <= depth[m]))
```

### `gen_bitlen()` — Bit Length Generation

Converts the tree structure into code lengths, enforcing the maximum
code length constraint:

```c
static void gen_bitlen(deflate_state *s, tree_desc *desc) {
    ct_data *tree = desc->dyn_tree;
    int max_code = desc->max_code;
    const ct_data *stree = desc->stat_desc->static_tree;
    const int *extra = desc->stat_desc->extra_bits;
    int base = desc->stat_desc->extra_base;
    int max_length = desc->stat_desc->max_length;
    int overflow = 0;

    // Traverse the tree via heap and set bit lengths
    for (int bits = 0; bits <= MAX_BITS; bits++)
        s->bl_count[bits] = 0;

    tree[s->heap[s->heap_max]].len = 0;  // Root has length 0

    for (int h = s->heap_max + 1; h < HEAP_SIZE; h++) {
        int n = s->heap[h];
        int bits = tree[tree[n].dad].len + 1;
        if (bits > max_length) {
            bits = max_length;
            overflow++;
        }
        tree[n].len = (uint16_t)bits;
        if (n > max_code) continue;  // Not a leaf

        s->bl_count[bits]++;
        // Account for extra bits in cost calculation
    }

    if (overflow == 0) return;

    // Adjust bit lengths to stay within max_length
    // Find the deepest non-full level and redistribute
    // ...
}
```

### `gen_codes()` — Code Generation

Converts bit lengths into canonical Huffman codes:

```c
static void gen_codes(ct_data *tree, int max_code, uint16_t *bl_count) {
    uint16_t next_code[MAX_BITS + 1];
    unsigned code = 0;

    // Step 1: Compute the first code for each bit length
    for (int bits = 1; bits <= MAX_BITS; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = (uint16_t)code;
    }

    // Step 2: Assign codes
    for (int n = 0; n <= max_code; n++) {
        int len = tree[n].len;
        if (len == 0) continue;
        tree[n].code = (uint16_t)bi_reverse(next_code[len]++, len);
    }
}
```

The `bi_reverse()` function reverses the bit order because DEFLATE uses
reversed (LSB-first) Huffman codes.

---

## Static Huffman Trees

DEFLATE defines fixed Huffman tables for BTYPE=01 blocks:

**Literal/Length codes**:
| Value | Bits | Codes |
|---|---|---|
| 0–143 | 8 | 00110000 – 10111111 |
| 144–255 | 9 | 110010000 – 111111111 |
| 256–279 | 7 | 0000000 – 0010111 |
| 280–287 | 8 | 11000000 – 11000111 |

**Distance codes**: All 30 codes use 5 bits (0–29).

Static tables are precomputed:
```c
static const ct_data static_ltree[L_CODES + 2];
static const ct_data static_dtree[D_CODES];
```

---

## Dynamic Tree Encoding

For BTYPE=10 blocks, the Huffman trees must be transmitted before the data.
DEFLATE encodes the trees themselves using a third Huffman tree (the
"bit-length tree").

### Tree Encoding Steps

1. **`scan_tree()`** — Find repeat patterns in the code length sequence:

```c
static void scan_tree(deflate_state *s, ct_data *tree, int max_code) {
    int prevlen = -1;
    int curlen;
    int nextlen = tree[0].len;
    int count = 0;
    int max_count = 7;
    int min_count = 4;

    for (int n = 0; n <= max_code; n++) {
        curlen = nextlen;
        nextlen = tree[n + 1].len;
        if (++count < max_count && curlen == nextlen) continue;

        if (count < min_count) {
            s->bl_tree[curlen].freq += count;
        } else if (curlen != 0) {
            if (curlen != prevlen) s->bl_tree[curlen].freq++;
            s->bl_tree[REP_3_6].freq++;       // Code 16: repeat 3-6 times
        } else if (count <= 10) {
            s->bl_tree[REPZ_3_10].freq++;      // Code 17: repeat 0, 3-10 times
        } else {
            s->bl_tree[REPZ_11_138].freq++;    // Code 18: repeat 0, 11-138 times
        }
        // Reset for next sequence
        count = 0;
        prevlen = curlen;
    }
}
```

Special codes for run-length encoding:
```c
#define REP_3_6     16    // Repeat previous length, 3-6 times (2 extra bits)
#define REPZ_3_10   17    // Repeat a zero length, 3-10 times (3 extra bits)
#define REPZ_11_138 18    // Repeat a zero length, 11-138 times (7 extra bits)
```

2. **`send_tree()`** — Emit the encoded tree to the bitstream:

```c
static void send_tree(deflate_state *s, ct_data *tree, int max_code) {
    int prevlen = -1;
    int curlen, nextlen = tree[0].len;
    int count = 0;

    for (int n = 0; n <= max_code; n++) {
        curlen = nextlen;
        nextlen = tree[n + 1].len;
        if (++count < max_count && curlen == nextlen) continue;

        if (count < min_count) {
            do { send_code(s, curlen, s->bl_tree); } while (--count);
        } else if (curlen != 0) {
            if (curlen != prevlen) {
                send_code(s, curlen, s->bl_tree);
                count--;
            }
            send_code(s, REP_3_6, s->bl_tree);
            send_bits(s, count - 3, 2);    // 2 extra bits
        } else if (count <= 10) {
            send_code(s, REPZ_3_10, s->bl_tree);
            send_bits(s, count - 3, 3);    // 3 extra bits
        } else {
            send_code(s, REPZ_11_138, s->bl_tree);
            send_bits(s, count - 11, 7);   // 7 extra bits
        }
        count = 0;
        prevlen = curlen;
    }
}
```

3. **Bit length code order** — The 19 bit-length codes are transmitted in
   a permuted order to minimize trailing zeros:

```c
static const uint8_t bl_order[BL_CODES] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};
```

---

## Bit Output Engine

### `send_bits()` — 64-bit Bit Buffer

From `trees_emit.h`:

```c
static inline void send_bits(deflate_state *s, int value, unsigned length) {
    s->bi_buf |= ((uint64_t)value << s->bi_valid);
    s->bi_valid += length;

    if (s->bi_valid >= BIT_BUF_SIZE) {  // BIT_BUF_SIZE = 64
        put_uint64(s, s->bi_buf);       // Flush 8 bytes to pending buffer
        s->bi_valid -= BIT_BUF_SIZE;
        s->bi_buf = (uint64_t)value >> (length - s->bi_valid);
    }
}
```

The 64-bit `bi_buf` accumulates bits and flushes complete 8-byte words to
the pending output buffer. This is more efficient than the traditional
16-bit buffer used in original zlib.

### `send_code()` — Emit a Huffman Code

```c
#define send_code(s, c, tree) send_bits(s, tree[c].code, tree[c].len)
```

### `bi_windup()` — Byte-Align the Output

```c
static inline void bi_windup(deflate_state *s) {
    if (s->bi_valid > 56) {
        put_uint64(s, s->bi_buf);
    } else {
        // Flush remaining bytes
        while (s->bi_valid >= 8) {
            put_byte(s, s->bi_buf & 0xff);
            s->bi_buf >>= 8;
            s->bi_valid -= 8;
        }
    }
    s->bi_buf = 0;
    s->bi_valid = 0;
}
```

---

## Block Emission

### `compress_block()`

Emits all symbols in the current block using Huffman codes:

```c
static void compress_block(deflate_state *s, const ct_data *ltree, const ct_data *dtree) {
    uint32_t sym_buf_index = 0;
    unsigned dist, lc, code;

    if (s->sym_next != 0) {
        do {
            // Read (distance, length/literal) from symbol buffer
            dist = s->sym_buf[sym_buf_index++];
            dist |= (uint32_t)s->sym_buf[sym_buf_index++] << 8;
            lc = s->sym_buf[sym_buf_index++];

            if (dist == 0) {
                // Literal byte
                send_code(s, lc, ltree);
            } else {
                // Length/distance pair
                code = zng_length_code[lc];
                send_code(s, code + LITERALS + 1, ltree);    // Length code
                int extra = extra_lbits[code];
                if (extra) send_bits(s, lc - base_length[code], extra);

                dist--;
                code = d_code(dist);
                send_code(s, code, dtree);                    // Distance code
                extra = extra_dbits[code];
                if (extra) send_bits(s, dist - base_dist[code], extra);
            }
        } while (sym_buf_index < s->sym_next);
    }

    send_code(s, END_BLOCK, ltree);   // Emit end-of-block
}
```

### Combined Base+Extra Tables

From `trees_emit.h`, base values and extra bits are combined into tables
for faster lookup:

```c
struct lut_pair {
    uint16_t base;
    uint8_t  extra;
};

// Length base values and extra bits (indexed by length code)
static const struct lut_pair base_length_lut[LENGTH_CODES] = {
    {0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, {7,0},
    {8,1}, {10,1}, {12,1}, {14,1},
    {16,2}, {20,2}, {24,2}, {28,2},
    {32,3}, {40,3}, {48,3}, {56,3},
    {64,4}, {80,4}, {96,4}, {112,4},
    {128,5}, {160,5}, {192,5}, {224,5}, {0,0}
};

// Distance base values and extra bits (indexed by distance code)
static const struct lut_pair base_dist_lut[D_CODES] = { ... };
```

---

## Block Type Selection

### `zng_tr_flush_block()`

Decides the block type (stored, static, or dynamic) and emits the block:

```c
void Z_INTERNAL zng_tr_flush_block(deflate_state *s, char *buf,
                                    uint32_t stored_len, int last) {
    uint32_t opt_lenb, static_lenb;
    int max_blindex = 0;

    if (s->level > 0) {
        // Build dynamic trees
        build_tree(s, &(s->l_desc));
        build_tree(s, &(s->d_desc));

        // Determine number of bit-length codes to send
        max_blindex = build_bl_tree(s);

        // Compute block sizes
        opt_lenb   = (s->opt_len + 3 + 7) >> 3;   // Dynamic block size
        static_lenb = (s->static_len + 3 + 7) >> 3; // Static block size
    } else {
        opt_lenb = static_lenb = stored_len + 5;
    }

    if (stored_len + 4 <= opt_lenb && buf != NULL) {
        // Stored block is smallest
        zng_tr_stored_block(s, buf, stored_len, last);
    } else if (s->strategy == Z_FIXED || static_lenb == opt_lenb) {
        // Static Huffman block
        send_bits(s, (STATIC_TREES << 1) + last, 3);
        compress_block(s, static_ltree, static_dtree);
    } else {
        // Dynamic Huffman block
        send_bits(s, (DYN_TREES << 1) + last, 3);
        send_all_trees(s, s->l_desc.max_code + 1,
                       s->d_desc.max_code + 1,
                       max_blindex + 1);
        compress_block(s, s->dyn_ltree, s->dyn_dtree);
    }

    init_block(s);  // Reset for next block

    if (last) bi_windup(s);  // Byte-align the final block
}
```

Block type constants:
```c
#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
```

### `init_block()` — Reset Tree State

```c
void Z_INTERNAL init_block(deflate_state *s) {
    // Reset literal/length frequencies
    for (int n = 0; n < L_CODES; n++)
        s->dyn_ltree[n].freq = 0;
    // Reset distance frequencies
    for (int n = 0; n < D_CODES; n++)
        s->dyn_dtree[n].freq = 0;
    // Reset bit-length frequencies
    for (int n = 0; n < BL_CODES; n++)
        s->bl_tree[n].freq = 0;

    s->dyn_ltree[END_BLOCK].freq = 1;
    s->opt_len = s->static_len = 0;
    s->sym_next = s->matches = 0;
}
```

---

## Symbol Buffer

During compression, literals and length/distance pairs are recorded in the
symbol buffer before Huffman encoding:

```c
// In deflate_state:
unsigned char *sym_buf;    // Buffer for literals and matches
uint32_t sym_next;         // Next free position
uint32_t sym_end;          // Size of sym_buf (lit_bufsize * 3)
```

Each symbol occupies 3 bytes:
- **Literal**: `{ 0x00, 0x00, byte_value }`
- **Match**: `{ dist_low, dist_high, length - STD_MIN_MATCH }`

```c
// Record a literal
static inline int zng_tr_tally_lit(deflate_state *s, unsigned char c) {
    s->sym_buf[s->sym_next++] = 0;
    s->sym_buf[s->sym_next++] = 0;
    s->sym_buf[s->sym_next++] = c;
    s->dyn_ltree[c].freq++;
    return (s->sym_next == s->sym_end);
}

// Record a match
static inline int zng_tr_tally_dist(deflate_state *s, uint32_t dist,
                                     uint32_t len) {
    s->sym_buf[s->sym_next++] = (uint8_t)(dist);
    s->sym_buf[s->sym_next++] = (uint8_t)(dist >> 8);
    s->sym_buf[s->sym_next++] = (uint8_t)len;
    s->matches++;
    dist--;
    s->dyn_ltree[zng_length_code[len] + LITERALS + 1].freq++;
    s->dyn_dtree[d_code(dist)].freq++;
    return (s->sym_next == s->sym_end);
}
```

When the buffer is full (`sym_next == sym_end`), the block is flushed.
