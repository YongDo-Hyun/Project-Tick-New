# genqrcode / libqrencode — Architecture

## High-Level Architecture

libqrencode is a layered C library that transforms input data into a QR Code bitmap through a pipeline of distinct modules. The architecture separates concerns cleanly: input management, bit stream encoding, error correction, frame construction, module placement, and masking are each handled by dedicated source files.

```
User Input
    │
    ▼
┌─────────────────────────────────────────┐
│  split.c — Input Splitter               │
│  Automatic mode detection & optimization│
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│  qrinput.c — Input Data Manager         │
│  QRinput linked list, mode encoders,    │
│  bit stream construction, padding       │
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│  bitstream.c — Bit Stream Class         │
│  Dynamic bit array with append ops      │
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│  qrencode.c — Core Encoder              │
│  QRRawCode / MQRRawCode, FrameFiller,   │
│  QRcode_encodeMask, interleaving        │
│                                         │
│  ┌──────────┐  ┌──────────────────────┐ │
│  │ rsecc.c  │  │ qrspec.c / mqrspec.c │ │
│  │ RS ECC   │  │ Spec tables & frames │ │
│  └──────────┘  └──────────────────────┘ │
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│  mask.c / mmask.c — Masking             │
│  8 patterns (QR) / 4 patterns (MQR),   │
│  penalty evaluation, mask selection     │
└─────────────┬───────────────────────────┘
              │
              ▼
         QRcode struct
    (version, width, data[])
```

---

## Module Dependency Graph

Each `.c` file includes specific headers. Here is the complete include dependency:

```
qrencode.c
├── qrencode.h
├── qrspec.h
├── mqrspec.h
├── bitstream.h
├── qrinput.h
├── rsecc.h
├── split.h
├── mask.h
└── mmask.h

qrinput.c
├── qrencode.h
├── qrspec.h
├── mqrspec.h
├── bitstream.h
└── qrinput.h

split.c
├── qrencode.h
├── qrinput.h
├── qrspec.h
└── split.h

mask.c
├── qrencode.h
├── qrspec.h
└── mask.h

mmask.c
├── qrencode.h
├── mqrspec.h
└── mmask.h

qrspec.c
├── qrspec.h
└── qrinput.h

mqrspec.c
└── mqrspec.h

rsecc.c
└── rsecc.h

bitstream.c
└── bitstream.h

qrenc.c (CLI tool)
└── qrencode.h
```

**Key insight:** `qrencode.c` is the integration point that pulls together all modules. The CLI tool (`qrenc.c`) only depends on `qrencode.h`, using exclusively the public API.

---

## Detailed Module Descriptions

### `qrencode.h` — Public API Header

This is the **only header that external consumers include**. It defines:

- `QRencodeMode` enum — All encoding modes (`QR_MODE_NUM`, `QR_MODE_AN`, `QR_MODE_8`, `QR_MODE_KANJI`, `QR_MODE_ECI`, `QR_MODE_FNC1FIRST`, `QR_MODE_FNC1SECOND`, plus internal `QR_MODE_NUL`, `QR_MODE_STRUCTURE`)
- `QRecLevel` enum — Error correction levels (`QR_ECLEVEL_L` through `QR_ECLEVEL_H`)
- `QRcode` struct — Output symbol (version, width, data array)
- `QRcode_List` struct — Singly-linked list of QRcode for structured append
- `QRinput` — Opaque typedef for input object
- `QRinput_Struct` — Opaque typedef for structured input set
- Version constants: `QRSPEC_VERSION_MAX` (40), `MQRSPEC_VERSION_MAX` (4)
- All `QRinput_*`, `QRcode_*`, and utility function declarations

The header uses `extern "C"` guards for C++ compatibility.

### `qrencode_inner.h` — Test-Only Internal Header

Exposes internal types and functions for the test suite:

```c
typedef struct {
    int dataLength;
    int eccLength;
    unsigned char *data;
    unsigned char *ecc;
} RSblock;

typedef struct {
    int version;
    int dataLength;
    int eccLength;
    unsigned char *datacode;
    unsigned char *ecccode;
    int b1;
    int blocks;
    RSblock *rsblock;
    int count;
} QRRawCode;

typedef struct {
    int version;
    int dataLength;
    int eccLength;
    unsigned char *datacode;
    unsigned char *ecccode;
    RSblock *rsblock;
    int oddbits;
    int count;
} MQRRawCode;
```

Exposed functions:
- `QRraw_new()`, `QRraw_getCode()`, `QRraw_free()`
- `MQRraw_new()`, `MQRraw_getCode()`, `MQRraw_free()`
- `FrameFiller_test()`, `FrameFiller_testMQR()`
- `QRcode_encodeMask()`, `QRcode_encodeMaskMQR()`
- `QRcode_new()`

### `qrencode.c` — Core Encoding Engine

This is the **central orchestrator** of the entire encoding process. It contains:

#### RSblock and QRRawCode

The `RSblock` struct represents one Reed-Solomon block:
```c
typedef struct {
    int dataLength;
    int eccLength;
    unsigned char *data;
    unsigned char *ecc;
} RSblock;
```

`QRRawCode` manages all RS blocks for a full QR Code:
```c
typedef struct {
    int version;
    int dataLength;
    int eccLength;
    unsigned char *datacode;   // merged data byte stream
    unsigned char *ecccode;    // merged ECC byte stream
    int b1;                    // number of type-1 blocks
    int blocks;                // total block count
    RSblock *rsblock;          // array of RS blocks
    int count;                 // iteration counter for getCode()
} QRRawCode;
```

`QRraw_new()` creates a QRRawCode from a QRinput:
1. Calls `QRinput_getByteStream()` to convert input to a padded byte stream
2. Retrieves the ECC specification via `QRspec_getEccSpec()`
3. Initializes RS blocks via `RSblock_init()`, which calls `RSECC_encode()` for each block
4. Sets up interleaving state (b1 records the count of type-1 blocks)

`QRraw_getCode()` implements data/ECC interleaving:
```c
STATIC_IN_RELEASE unsigned char QRraw_getCode(QRRawCode *raw)
{
    int col, row;
    unsigned char ret;

    if(raw->count < raw->dataLength) {
        row = raw->count % raw->blocks;
        col = raw->count / raw->blocks;
        if(col >= raw->rsblock[0].dataLength) {
            row += raw->b1;
        }
        ret = raw->rsblock[row].data[col];
    } else if(raw->count < raw->dataLength + raw->eccLength) {
        row = (raw->count - raw->dataLength) % raw->blocks;
        col = (raw->count - raw->dataLength) / raw->blocks;
        ret = raw->rsblock[row].ecc[col];
    } else {
        return 0;
    }
    raw->count++;
    return ret;
}
```

This interleaves by cycling through blocks row-by-row: the first byte from each block, then the second byte from each block, and so on. When type-1 blocks are exhausted (their data is shorter), it shifts to type-2 blocks by adding `b1` to the row index.

#### FrameFiller

The `FrameFiller` struct manages the zigzag placement of data modules:

```c
typedef struct {
    int width;
    unsigned char *frame;
    int x, y;     // current position
    int dir;      // direction: -1 (upward) or 1 (downward)
    int bit;      // 0 or 1 within a column pair
    int mqr;      // flag for Micro QR mode
} FrameFiller;
```

`FrameFiller_set()` initializes the filler at the bottom-right corner `(width-1, width-1)` with direction `-1` (upward).

`FrameFiller_next()` implements the QR Code module placement algorithm:
- Modules are placed in 2-column strips from right to left
- Within each strip, columns alternate right-left
- Direction alternates between upward and downward
- Column 6 (the vertical timing pattern) is skipped in full QR mode
- Modules already marked with bit 7 set (`0x80`) — function patterns — are skipped

#### QRcode_encodeMask()

The main encoding function for full QR Codes:

1. Validates input (not MQR, version in range, valid EC level)
2. Creates `QRRawCode` from input
3. Creates base frame via `QRspec_newFrame()`
4. Places data bits using `FrameFiller_next()`:
   - Data modules: `*p = ((bit & code) != 0)` — only LSB set
   - ECC modules: `*p = 0x02 | ((bit & code) != 0)` — bit 1 also set
   - Remainder bits: `*p = 0x02`
5. Applies masking:
   - `mask == -2`: debug mode, no masking
   - `mask < 0` (normal): `Mask_mask()` evaluates all 8 patterns
   - `mask >= 0`: `Mask_makeMask()` applies specific mask

#### QRcode_encodeMaskMQR()

The Micro QR variant:
- Validates MQR mode, version 1–4, EC level L through Q
- Uses `MQRRawCode` instead of `QRRawCode`
- Handles `oddbits` — the last data byte may have fewer than 8 significant bits
- Uses `MMask_mask()` / `MMask_makeMask()` for the 4-pattern MQR masking
- Sets FrameFiller's `mqr` flag to 1 (no column-6 skip)

#### High-Level Encoding Functions

`QRcode_encodeInput()` simply dispatches based on the `mqr` flag:
```c
QRcode *QRcode_encodeInput(QRinput *input)
{
    if(input->mqr) {
        return QRcode_encodeMaskMQR(input, -1);
    } else {
        return QRcode_encodeMask(input, -1);
    }
}
```

`QRcode_encodeStringReal()` is the shared implementation for `QRcode_encodeString()` and `QRcode_encodeStringMQR()`:
1. Creates a `QRinput` (MQR or standard)
2. Calls `Split_splitStringToQRinput()` for automatic mode optimization
3. Calls `QRcode_encodeInput()`

For MQR, `QRcode_encodeStringMQR()` tries each version from the minimum up to 4:
```c
for(i = version; i <= MQRSPEC_VERSION_MAX; i++) {
    QRcode *code = QRcode_encodeStringReal(string, i, level, 1, hint, casesensitive);
    if(code != NULL) return code;
}
```

#### Structured Append

`QRcode_encodeInputStructured()` encodes each `QRinput` in a `QRinput_Struct` and builds a `QRcode_List` linked list.

`QRcode_encodeStringStructured()` and variants auto-split the input, insert structured append headers, encode each part, and return the linked list.

---

### `qrinput.h` / `qrinput.c` — Input Data Management

This module manages the input data pipeline from raw user data to a padded byte stream ready for RS encoding.

#### Internal Structures

```c
// A linked list entry for one data chunk
struct _QRinput_List {
    QRencodeMode mode;     // encoding mode
    int size;              // data size in bytes
    unsigned char *data;   // data chunk
    BitStream *bstream;    // encoded bit stream (created during encoding)
    QRinput_List *next;    // next chunk
};

// The main input object
struct _QRinput {
    int version;
    QRecLevel level;
    QRinput_List *head;    // first data chunk
    QRinput_List *tail;    // last data chunk
    int mqr;               // 1 if Micro QR mode
    int fnc1;              // FNC1 mode flag
    unsigned char appid;   // FNC1 application ID
};

// Structured append management
struct _QRinput_Struct {
    int size;              // number of symbols
    int parity;            // parity byte
    QRinput_InputList *head;
    QRinput_InputList *tail;
};
```

#### Mode Encoding Functions

Each encoding mode has three functions in `qrinput.c`:

1. **Check function** — Validates input data for the mode
2. **Estimate function** — Estimates bit count without actually encoding
3. **Encode function** — Produces the actual bit stream

##### Numeric Mode (`QRinput_encodeModeNum`)

Encodes digits in groups:
- 3 digits → 10 bits (values 000–999)
- 2 remaining digits → 7 bits
- 1 remaining digit → 4 bits

```c
int QRinput_estimateBitsModeNum(int size)
{
    int w = size / 3;
    int bits = w * 10;
    switch(size - w * 3) {
        case 1: bits += 4; break;
        case 2: bits += 7; break;
    }
    return bits;
}
```

##### Alphanumeric Mode (`QRinput_encodeModeAn`)

Uses the 45-character lookup table `QRinput_anTable[128]`:
- Pairs → 11 bits (value = c1 × 45 + c2)
- Odd character → 6 bits

The table maps: 0-9 → 0-9, A-Z → 10-35, space → 36, $ → 37, % → 38, * → 39, + → 40, - → 41, . → 42, / → 43, : → 44.

The lookup macro:
```c
#define QRinput_lookAnTable(__c__) \
    ((__c__ & 0x80)?-1:QRinput_anTable[(int)__c__])
```

##### 8-Bit Mode (`QRinput_encodeMode8`)

Each byte → 8 bits, using `BitStream_appendBytes()`.

##### Kanji Mode (`QRinput_encodeModeKanji`)

Shift-JIS double-byte characters are compressed:
1. If code ≤ 0x9FFC: subtract 0x8140
2. If code > 0x9FFC: subtract 0xC140
3. High byte × 0xC0 + low byte → 13-bit value

Validation in `QRinput_checkModeKanji()`:
```c
if(val < 0x8140 || (val > 0x9ffc && val < 0xe040) || val > 0xebbf) {
    return -1;
}
```

##### ECI Mode (`QRinput_encodeModeECI`)

ECI indicator encoding (per JIS X0510:2004, Table 4):
- 0–127: 1 byte (8 bits)
- 128–16383: 2 bytes (16 bits, prefix 0x8000)
- 16384–999999: 3 bytes (24 bits, prefix 0xC0000)

##### Structured Append Header

20-bit header: 4 bits mode indicator + 4 bits symbol count + 4 bits symbol index + 8 bits parity.

##### FNC1 Second Position

4-bit mode indicator + 8-bit application ID.

#### Bit Stream Construction Pipeline

The conversion from input chunks to a byte stream follows this call chain:

```
QRinput_getByteStream()
    └── QRinput_getBitStream()
            ├── QRinput_convertData()    [for standard QR]
            │       ├── QRinput_estimateVersion()
            │       │       └── QRinput_estimateBitStreamSize()
            │       │               └── QRinput_estimateBitStreamSizeOfEntry() × N
            │       └── QRinput_createBitStream()
            │               └── QRinput_encodeBitStream() × N
            │                       └── QRinput_encodeMode{Num,An,8,Kanji,...}()
            ├── QRinput_appendPaddingBit()    [for standard QR]
            └── QRinput_appendPaddingBitMQR() [for Micro QR]
                    └── BitStream_toByte()
```

`QRinput_convertData()` handles version auto-selection with a convergence loop:
```c
for(;;) {
    BitStream_reset(bstream);
    bits = QRinput_createBitStream(input, bstream);
    ver = QRspec_getMinimumVersion((bits + 7) / 8, input->level);
    if(ver > QRinput_getVersion(input)) {
        QRinput_setVersion(input, ver);
    } else {
        break;
    }
}
```

`QRinput_encodeBitStream()` handles entry splitting when data exceeds `QRspec_maximumWords()`:
```c
if(words != 0 && entry->size > words) {
    st1 = QRinput_List_newEntry(entry->mode, words, entry->data);
    st2 = QRinput_List_newEntry(entry->mode, entry->size - words, &entry->data[words]);
    QRinput_encodeBitStream(st1, bstream, version, mqr);
    QRinput_encodeBitStream(st2, bstream, version, mqr);
}
```

Padding appends:
1. Terminator (up to 4 zero bits)
2. Byte-alignment zeros
3. Alternating pad codewords: `0xEC`, `0x11`, `0xEC`, `0x11`, ...

---

### `bitstream.h` / `bitstream.c` — Binary Sequence Class

A dynamic bit array class used throughout the encoding pipeline.

```c
typedef struct {
    size_t length;     // current number of bits
    size_t datasize;   // allocated buffer size
    unsigned char *data; // one byte per bit (0 or 1)
} BitStream;
```

**Critical design choice:** Each bit occupies one byte in memory. This simplifies bit manipulation at the cost of memory. The buffer starts at `DEFAULT_BUFSIZE` (128) and doubles on demand via `BitStream_expand()`.

Key operations:

| Function | Description |
|---|---|
| `BitStream_new()` | Allocate with 128-byte initial buffer |
| `BitStream_append(dst, src)` | Append another BitStream |
| `BitStream_appendNum(bs, bits, num)` | Append `bits` bits of integer `num` |
| `BitStream_appendBytes(bs, size, data)` | Append `size` bytes (8 bits each) |
| `BitStream_toByte(bs)` | Pack bit array into byte array |
| `BitStream_free(bs)` | Free all memory |

Macros:
```c
#define BitStream_size(__bstream__) (__bstream__->length)
#define BitStream_reset(__bstream__) (__bstream__->length = 0)
```

`BitStream_toByte()` packs the 1-byte-per-bit representation into a proper byte array:
```c
for(i = 0; i < bytes; i++) {
    v = 0;
    for(j = 0; j < 8; j++) {
        v = (unsigned char)(v << 1);
        v |= *p;
        p++;
    }
    data[i] = v;
}
```

---

### `qrspec.h` / `qrspec.c` — QR Code Specification Tables

Contains all specification-derived data tables and frame construction for full QR Codes.

#### Capacity Table

```c
typedef struct {
    int width;
    int words;
    int remainder;
    int ec[4];
} QRspec_Capacity;

static const QRspec_Capacity qrspecCapacity[QRSPEC_VERSION_MAX + 1];
```

Sourced from Table 1 (p.13) and Tables 12–16 (pp.30–36) of JIS X0510:2004.

#### Length Indicator Table

```c
static const int lengthTableBits[4][3] = {
    {10, 12, 14},  // Numeric
    { 9, 11, 13},  // Alphanumeric
    { 8, 16, 16},  // 8-bit
    { 8, 10, 12}   // Kanji
};
```

Three version ranges: 1–9, 10–26, 27–40.

#### ECC Block Specification Table

```c
static const int eccTable[QRSPEC_VERSION_MAX+1][4][2];
```

Each entry `eccTable[version][level]` gives `{type1_blocks, type2_blocks}`. Combined with `QRspec_getEccSpec()` to produce the 5-element spec array:

```c
void QRspec_getEccSpec(int version, QRecLevel level, int spec[5])
```

Where `spec` = `{num_type1_blocks, type1_data_codes, ecc_codes_per_block, num_type2_blocks, type2_data_codes}`.

Accessor macros:
```c
#define QRspec_rsBlockNum(__spec__)      (__spec__[0] + __spec__[3])
#define QRspec_rsBlockNum1(__spec__)     (__spec__[0])
#define QRspec_rsDataCodes1(__spec__)    (__spec__[1])
#define QRspec_rsEccCodes1(__spec__)     (__spec__[2])
#define QRspec_rsBlockNum2(__spec__)     (__spec__[3])
#define QRspec_rsDataCodes2(__spec__)    (__spec__[4])
#define QRspec_rsEccCodes2(__spec__)     (__spec__[2])
```

Note: type-1 and type-2 blocks share the same ECC code count.

#### Alignment Pattern Table

```c
static const int alignmentPattern[QRSPEC_VERSION_MAX+1][2];
```

From Table 1 in Appendix E (p.71). Stores the second and third alignment pattern positions; remaining positions are interpolated.

`QRspec_putAlignmentPattern()` places all alignment markers, computing positions from the stored two values and the inter-pattern distance.

#### Version Information Pattern

```c
static const unsigned int versionPattern[QRSPEC_VERSION_MAX - 6];
```

BCH-encoded version information for versions 7–40. From Appendix D (p.68).

#### Format Information

```c
static const unsigned int formatInfo[4][8];
```

BCH-encoded format information indexed by `[level][mask]`.

#### Frame Creation (`QRspec_createFrame`)

Builds the initial symbol frame for a given version:

1. Allocates `width × width` bytes, zeroed
2. Places **3 finder patterns** (7×7, `0xC1`/`0xC0` pattern) at corners
3. Places **separators** (1-module-wide white border around finders, `0xC0`)
4. Masks **format information area** (9+8 modules around finder, `0x84`)
5. Places **timing patterns** (alternating `0x91`/`0x90` along row 6 and column 6)
6. Places **alignment patterns** (5×5, `0xA1`/`0xA0` pattern)
7. For versions ≥ 7: places **version information** (6×3 blocks, `0x88`/`0x89`)
8. Sets the **dark module** at position `(8, width-8)` to `0x81`

All function pattern modules have bit 7 set (`0x80`), so the FrameFiller skips them during data placement.

---

### `mqrspec.h` / `mqrspec.c` — Micro QR Specification Tables

Parallel to `qrspec.c` but for Micro QR (versions M1–M4).

```c
typedef struct {
    int width;
    int ec[4];
} MQRspec_Capacity;

static const MQRspec_Capacity mqrspecCapacity[MQRSPEC_VERSION_MAX + 1] = {
    {  0, {0,  0,  0, 0}},
    { 11, {2,  0,  0, 0}},   // M1
    { 13, {5,  6,  0, 0}},   // M2
    { 15, {6,  8,  0, 0}},   // M3
    { 17, {8, 10, 14, 0}}    // M4
};
```

Notable difference: `MQRspec_getDataLengthBit()` returns data capacity in **bits** (not bytes), because Micro QR symbols can have non-byte-aligned data areas:
```c
int MQRspec_getDataLengthBit(int version, QRecLevel level)
{
    int w = mqrspecCapacity[version].width - 1;
    int ecc = mqrspecCapacity[version].ec[level];
    if(ecc == 0) return 0;
    return w * w - 64 - ecc * 8;
}
```

The data length in bytes rounds up: `(bits + 4) / 8`.

#### Micro QR Frame Creation

`MQRspec_createFrame()` is simpler than the full QR version:
- Only **1 finder pattern** (top-left)
- No alignment patterns
- No version information
- Timing patterns run along one row and one column from the finder

#### Format Info Type Table

```c
static const int typeTable[MQRSPEC_VERSION_MAX + 1][3] = {
    {-1, -1, -1},
    { 0, -1, -1},   // M1: only error detection
    { 1,  2, -1},   // M2: L, M
    { 3,  4, -1},   // M3: L, M
    { 5,  6,  7}    // M4: L, M, Q
};
```

Maps `(version, level)` → format info table index. Returns -1 for unsupported combinations.

---

### `rsecc.h` / `rsecc.c` — Reed-Solomon Error Correction

Contains the GF(2^8) arithmetic and RS encoding for QR Codes.

Single public function:
```c
int RSECC_encode(size_t data_length, size_t ecc_length,
                 const unsigned char *data, unsigned char *ecc);
```

Internal state:
- `alpha[256]` — Power-to-element mapping (logarithm table)
- `aindex[256]` — Element-to-power mapping (antilogarithm table)
- `generator[29][31]` — Cached generator polynomials for ECC lengths 2–30
- `generatorInitialized[29]` — Whether each generator has been computed

Lazy initialization via `RSECC_init()` and `generator_init()`, protected by `RSECC_mutex` when pthreads are available.

The primitive polynomial is `0x11d` = $x^8 + x^4 + x^3 + x^2 + 1$, per JIS X0510:2004 p.37.

See [reed-solomon.md](reed-solomon.md) for detailed implementation analysis.

---

### `split.h` / `split.c` — Input String Splitter

Automatically parses an input string and splits it into optimal encoding mode segments.

Entry point:
```c
int Split_splitStringToQRinput(const char *string, QRinput *input,
                               QRencodeMode hint, int casesensitive);
```

Key functions:
- `Split_identifyMode()` — Classifies a character: digit → `QR_MODE_NUM`, AN table match → `QR_MODE_AN`, Shift-JIS → `QR_MODE_KANJI`, else → `QR_MODE_8`
- `Split_eatNum()` — Consumes a run of numeric characters, considers switching to AN or 8-bit if more efficient
- `Split_eatAn()` — Consumes alphanumeric characters, embedded digit runs tested for mode-switch optimization
- `Split_eatKanji()` — Consumes pairs of Kanji bytes
- `Split_eat8()` — Consumes 8-bit characters, tests for switching to NUM or AN when sub-runs are encountered

The optimization logic compares bit costs: each `Split_eat*` function calculates `dif` — the bit savings of staying in the current mode vs. switching. Example from `Split_eatNum()`:

```c
dif = QRinput_estimateBitsModeNum(run) + 4 + ln
    + QRinput_estimateBitsMode8(1)
    - QRinput_estimateBitsMode8(run + 1);
if(dif > 0) {
    return Split_eat8(string, input, hint);
}
```

Case conversion (when `casesensitive=0`) is handled by `dupAndToUpper()`, which converts lowercase to uppercase while preserving Kanji double-byte sequences.

---

### `mask.h` / `mask.c` — QR Code Masking

Implements the 8 mask patterns for full QR Code and the penalty evaluation algorithm.

#### Mask Patterns

All 8 patterns are defined via the `MASKMAKER` macro:

```c
#define MASKMAKER(__exp__) \
    int x, y;\
    int b = 0;\
    for(y = 0; y < width; y++) {\
        for(x = 0; x < width; x++) {\
            if(*s & 0x80) {\
                *d = *s;\
            } else {\
                *d = *s ^ ((__exp__) == 0);\
            }\
            b += (int)(*d & 1);\
            s++; d++;\
        }\
    }\
    return b;
```

The 8 mask functions:

| Pattern | Function | Condition (dark if true) |
|---|---|---|
| 0 | `Mask_mask0` | `(x+y) % 2 == 0` |
| 1 | `Mask_mask1` | `y % 2 == 0` |
| 2 | `Mask_mask2` | `x % 3 == 0` |
| 3 | `Mask_mask3` | `(x+y) % 3 == 0` |
| 4 | `Mask_mask4` | `((y/2)+(x/3)) % 2 == 0` |
| 5 | `Mask_mask5` | `(x*y)%2 + (x*y)%3 == 0` |
| 6 | `Mask_mask6` | `((x*y)%2 + (x*y)%3) % 2 == 0` |
| 7 | `Mask_mask7` | `((x*y)%3 + (x+y)%2) % 2 == 0` |

Function pointer array: `static MaskMaker *maskMakers[8]`

#### Penalty Evaluation

`Mask_mask()` tries all 8 masks and selects the one with the lowest penalty:

```c
for(i = 0; i < maskNum; i++) {
    penalty = 0;
    blacks = maskMakers[i](width, frame, mask);
    blacks += Mask_writeFormatInformation(width, mask, i, level);
    bratio = (200 * blacks + w2) / w2 / 2;
    penalty = (abs(bratio - 50) / 5) * N4;
    penalty += Mask_evaluateSymbol(width, mask);
    if(penalty < minPenalty) {
        minPenalty = penalty;
        memcpy(bestMask, mask, w2);
    }
}
```

Penalty constants from JIS X0510:2004, Section 8.8.2:
```c
#define N1 (3)   // Run penalty base
#define N2 (3)   // 2×2 block penalty
#define N3 (40)  // Finder-like pattern penalty
#define N4 (10)  // Proportion penalty per 5% deviation
```

See [masking-algorithms.md](masking-algorithms.md) for detailed penalty calculation analysis.

---

### `mmask.h` / `mmask.c` — Micro QR Masking

Implements 4 mask patterns for Micro QR Code with a different evaluation algorithm.

The 4 patterns:

| Pattern | Condition |
|---|---|
| 0 | `y % 2 == 0` |
| 1 | `((y/2)+(x/3)) % 2 == 0` |
| 2 | `((x*y)%2 + (x*y)%3) % 2 == 0` |
| 3 | `((x+y)%2 + (x*y)%3) % 2 == 0` |

The Micro QR evaluation in `MMask_evaluateSymbol()` uses a completely different approach from full QR:
```c
STATIC_IN_RELEASE int MMask_evaluateSymbol(int width, unsigned char *frame)
{
    int x, y;
    unsigned char *p;
    int sum1 = 0, sum2 = 0;

    p = frame + width * (width - 1);
    for(x = 1; x < width; x++) {
        sum1 += (p[x] & 1);
    }

    p = frame + width * 2 - 1;
    for(y = 1; y < width; y++) {
        sum2 += (*p & 1);
        p += width;
    }

    return (sum1 <= sum2)?(sum1 * 16 + sum2):(sum2 * 16 + sum1);
}
```

Instead of penalties, it counts dark modules on the bottom row and right column, then selects the mask with the **highest** score (not lowest).

---

## Data Flow: Complete Encoding Pipeline

Here is the detailed function call chain for encoding a string:

```
QRcode_encodeString("Hello", 0, QR_ECLEVEL_M, QR_MODE_8, 1)
  │
  └── QRcode_encodeStringReal("Hello", 0, QR_ECLEVEL_M, 0, QR_MODE_8, 1)
        │
        ├── QRinput_new2(0, QR_ECLEVEL_M)
        │     └── allocate QRinput {version=0, level=M, mqr=0}
        │
        ├── Split_splitStringToQRinput("Hello", input, QR_MODE_8, 1)
        │     └── Split_splitString("Hello", input, QR_MODE_8)
        │           ├── Split_identifyMode("H") → QR_MODE_AN
        │           └── Split_eatAn("Hello", ...)
        │                 └── QRinput_append(input, QR_MODE_AN, 5, "Hello")
        │
        └── QRcode_encodeInput(input)
              │
              └── QRcode_encodeMask(input, -1)
                    │
                    ├── QRraw_new(input)
                    │     ├── QRinput_getByteStream(input)
                    │     │     ├── QRinput_convertData() — version auto-select
                    │     │     ├── QRinput_createBitStream() — encode each chunk
                    │     │     ├── QRinput_appendPaddingBit() — terminator + padding
                    │     │     └── BitStream_toByte() — pack bits to bytes
                    │     │
                    │     ├── QRspec_getEccSpec(version, level, spec)
                    │     └── RSblock_init() → RSECC_encode() per block
                    │
                    ├── QRspec_newFrame(version)
                    │     └── QRspec_createFrame() — finder, timing, alignment
                    │
                    ├── FrameFiller placement loop
                    │     └── FrameFiller_next() × (dataLength + eccLength) × 8
                    │
                    └── Mask_mask(width, frame, level)
                          ├── maskMakers[0..7]() — apply each pattern
                          ├── Mask_writeFormatInformation() — embed format info
                          ├── Mask_evaluateSymbol() — penalty calculation
                          │     ├── Mask_calcN2() — 2×2 blocks
                          │     ├── Mask_calcRunLengthH() — horizontal runs
                          │     ├── Mask_calcRunLengthV() — vertical runs
                          │     └── Mask_calcN1N3() — run + finder penalties
                          └── select minimum penalty mask
```

---

## STATIC_IN_RELEASE Pattern

The codebase uses the `STATIC_IN_RELEASE` macro to control visibility:

```c
// When WITH_TESTS is defined:
#define STATIC_IN_RELEASE

// When WITH_TESTS is not defined:
#define STATIC_IN_RELEASE static
```

Functions marked `STATIC_IN_RELEASE` (like `QRraw_new`, `QRcode_encodeMask`, `Mask_evaluateSymbol`) are `static` in release builds and externally visible in test builds. Similarly, `#ifdef WITH_TESTS` blocks expose additional test-only functions.

---

## Memory Management

The library follows a consistent pattern:
- All allocation uses `malloc()`/`calloc()`/`realloc()`
- Every `_new()` function has a corresponding `_free()` function
- On allocation failure, functions return `NULL` and set `errno`
- `QRcode_free()` frees both the `QRcode` struct and its internal `data` array
- `QRinput_free()` walks the linked list and frees each entry
- `BitStream_free()` frees the data buffer and the struct

No memory pools or custom allocators are used.

---

## Error Handling

Errors are reported via return values and `errno`:

| Error | Meaning |
|---|---|
| `EINVAL` | Invalid argument (bad version, level, mode, or data) |
| `ENOMEM` | Memory allocation failure |
| `ERANGE` | Input data too large for any supported version |

Functions returning pointers return `NULL` on error. Functions returning `int` return `-1` on error and `0` on success. The only exception is `QRraw_getCode()` which returns `0` when the code stream is exhausted.
