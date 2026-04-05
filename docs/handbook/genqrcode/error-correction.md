# genqrcode / libqrencode — Error Correction

## Overview

QR Code uses Reed-Solomon error correction to enable reliable scanning even when parts of the symbol are damaged or obscured. libqrencode implements error correction through the `rsecc.c` module (GF(2^8) Reed-Solomon encoder) and coordinates block layout through `qrspec.c` / `mqrspec.c`.

---

## Error Correction Levels

Four levels are defined in `QRecLevel`:

```c
typedef enum {
    QR_ECLEVEL_L = 0,  // Low      — ~7% recovery
    QR_ECLEVEL_M,      // Medium   — ~15% recovery
    QR_ECLEVEL_Q,      // Quartile — ~25% recovery
    QR_ECLEVEL_H       // High     — ~30% recovery
} QRecLevel;
```

Higher error correction means more codewords are devoted to ECC, reducing data capacity.

### Micro QR Restrictions

Not all Micro QR versions support all levels:

| Version | Supported Levels |
|---|---|
| M1 | Error detection only (no EC level parameter effect) |
| M2 | L, M |
| M3 | L, M |
| M4 | L, M, Q |

Micro QR never supports `QR_ECLEVEL_H`. This is enforced in `QRinput_newMQR()` via `MQRspec_getECCLength()` returning 0 for invalid combinations.

---

## ECC Specification Tables

### Full QR Code — `eccTable`

Defined in `qrspec.c`:

```c
static const int eccTable[QRSPEC_VERSION_MAX+1][4][2] = {
    {{ 0,  0}, { 0,  0}, { 0,  0}, { 0,  0}},  // version 0 (unused)
    {{ 1,  0}, { 1,  0}, { 1,  0}, { 1,  0}},  // version 1
    {{ 1,  0}, { 1,  0}, { 1,  0}, { 1,  0}},  // version 2
    {{ 1,  0}, { 1,  0}, { 2,  0}, { 2,  0}},  // version 3
    // ... through version 40
};
```

Dimensions: `[version][ec_level][2]`. The two values are:
- `eccTable[v][l][0]` — Number of RS blocks in group 1
- `eccTable[v][l][1]` — Number of RS blocks in group 2 (0 if only one group)

### ECC Spec Extraction

`QRspec_getEccSpec()` computes a 5-element spec array:

```c
void QRspec_getEccSpec(int version, QRecLevel level, int spec[5])
{
    int b1 = eccTable[version][level][0];
    int b2 = eccTable[version][level][1];
    int data = QRspec_getDataLength(version, level);
    int ecc  = QRspec_getECCLength(version, level);

    if(b2 == 0) {
        spec[0] = b1;
        spec[1] = data / b1;
        spec[2] = ecc / b1;
        spec[3] = 0;
        spec[4] = 0;
    } else {
        spec[0] = b1;
        spec[1] = data / (b1 + b2);
        spec[2] = ecc  / (b1 + b2);
        spec[3] = b2;
        spec[4] = spec[1] + 1;
    }
}
```

The spec layout:
- `spec[0]` — Number of RS blocks in group 1 (`b1`)
- `spec[1]` — Data codewords per block in group 1
- `spec[2]` — ECC codewords per block
- `spec[3]` — Number of RS blocks in group 2 (`b2`, 0 if none)
- `spec[4]` — Data codewords per block in group 2 (= spec[1] + 1, or 0)

Accessor macros:
```c
#define QRspec_rsBlockNum(__spec__)    (__spec__[0] + __spec__[3])
#define QRspec_rsBlockNum1(__spec__)   (__spec__[0])
#define QRspec_rsDataCodes1(__spec__)  (__spec__[1])
#define QRspec_rsEccCodes1(__spec__)   (__spec__[2])
#define QRspec_rsBlockNum2(__spec__)   (__spec__[3])
#define QRspec_rsDataCodes2(__spec__)  (__spec__[4])
#define QRspec_rsEccCodes2(__spec__)   (__spec__[2])
```

Note that both groups share the same ECC codeword count.

---

## Capacity Tables

### Full QR — `qrspecCapacity`

Defined in `qrspec.c`, 41 entries (index 0 is unused):

```c
typedef struct {
    int width;       // symbol width in modules
    int words;       // total data codewords
    int remainder;   // remainder bits (0–7)
    int ec[4];       // data codewords per EC level [L, M, Q, H]
} QRspec_Capacity;

static const QRspec_Capacity qrspecCapacity[QRSPEC_VERSION_MAX + 1] = {
    {  0,    0, 0, {   0,    0,    0,    0}},
    { 21,   26, 0, {  19,   16,   13,    9}},  // v1: 21×21
    { 25,   44, 7, {  34,   28,   22,   16}},  // v2: 25×25
    { 29,   70, 7, {  55,   44,   34,   26}},  // v3
    { 33,  100, 7, {  80,   64,   48,   36}},  // v4
    // ... through version 40
    {177, 3706, 0, {2956, 2334, 1666, 1276}},  // v40: 177×177
};
```

Key accessor functions:
```c
int QRspec_getDataLength(int version, QRecLevel level);
// Returns qrspecCapacity[version].ec[level]

int QRspec_getECCLength(int version, QRecLevel level);
// Returns qrspecCapacity[version].words - qrspecCapacity[version].ec[level]

int QRspec_getWidth(int version);
// Returns qrspecCapacity[version].width (= version * 4 + 17)

int QRspec_getRemainder(int version);
// Returns qrspecCapacity[version].remainder

int QRspec_getMinimumVersion(int size, QRecLevel level);
// Scans versions 1-40 for first where data capacity >= size
```

### Sample Capacities

| Version | Width | Total Words | L Data | M Data | Q Data | H Data |
|---|---|---|---|---|---|---|
| 1 | 21 | 26 | 19 | 16 | 13 | 9 |
| 5 | 37 | 134 | 108 | 86 | 62 | 46 |
| 10 | 57 | 346 | 271 | 213 | 151 | 119 |
| 20 | 97 | 1022 | 858 | 666 | 482 | 382 |
| 40 | 177 | 3706 | 2956 | 2334 | 1666 | 1276 |

### Micro QR — `mqrspecCapacity`

Defined in `mqrspec.c`, 5 entries:

```c
static const MQRspec_Capacity mqrspecCapacity[MQRSPEC_VERSION_MAX + 1] = {
    {0,  0, {0, 0, 0, 0}},
    {11, 5, {2, 0, 0, 0}},    // M1: 11×11, detection only (2 data + 3 ECC)
    {13, 10, {5, 4, 0, 0}},   // M2: 13×13
    {15, 17, {11, 9, 7, 0}},  // M3: 15×15
    {17, 24, {16, 14, 10, 0}} // M4: 17×17
};
```

---

## RSblock Structure

In `qrencode.c`, RS blocks are organized in two structures:

```c
typedef struct {
    int dataLength;
    unsigned char *data;
    int eccLength;
    unsigned char *ecc;
} RSblock;
```

### QRRawCode — Full QR

```c
typedef struct {
    int dataLength;
    int eccLength;
    int b1;           // number of group 1 blocks
    int b2;           // number of group 2 blocks
    int rsblock_num;  // total blocks (b1 + b2)
    RSblock *rsblock;
    int count;        // interleave counter
} QRRawCode;
```

### MQRRawCode — Micro QR

```c
typedef struct {
    int version;
    int dataLength;
    int eccLength;
    unsigned char *datacode;
    unsigned char *ecccode;
    int b1;
    int rsblock_num;
    RSblock *rsblock;
    int count;
    int oddbits;
} MQRRawCode;
```

The `oddbits` field handles Micro QR versions where data length is specified in bits rather than bytes (M1 has odd-bit data length).

### Block Initialization

`RSblock_initBlock()` calls the Reed-Solomon encoder for each block:

```c
static int RSblock_initBlock(RSblock *block, int dl, unsigned char *data,
                             int el, unsigned char *ecc,
                             RSECC_encoder encoder)
{
    block->dataLength = dl;
    block->data = data;
    block->eccLength = el;
    block->ecc = ecc;
    return encoder(dl, el, data, ecc);
}
```

Where `encoder` is a function pointer to `RSECC_encode`.

---

## Block Interleaving

The `QRraw_getCode()` function interleaves data and ECC bytes across blocks:

```c
unsigned char QRraw_getCode(QRRawCode *raw)
{
    if(raw->count < raw->dataLength) {
        // Data interleaving phase
        int blockNum = raw->count % raw->rsblock_num;
        int dataPos  = raw->count / raw->rsblock_num;
        // ... skip blocks whose data is too short for this position ...
        unsigned char code = raw->rsblock[blockNum].data[dataPos];
    } else {
        // ECC interleaving phase
        int blockNum = (raw->count - raw->dataLength) % raw->rsblock_num;
        int eccPos   = (raw->count - raw->dataLength) / raw->rsblock_num;
        unsigned char code = raw->rsblock[blockNum].ecc[eccPos];
    }
    raw->count++;
    return code;
}
```

The interleaving ensures that consecutive codewords in the symbol come from different RS blocks, maximizing burst-error recovery.

For a symbol with 2 blocks of 15 data + 1 block of 16 data:
- First pass: byte 0 from block 0, byte 0 from block 1, byte 0 from block 2
- Second pass: byte 1 from block 0, byte 1 from block 1, byte 1 from block 2
- ...
- The extra byte from block 2 (position 15) is appended at the end

---

## The RSECC Module

The core Reed-Solomon encoder in `rsecc.c`.

### Galois Field GF(2^8)

**Primitive polynomial**: `proot = 0x11d` = x^8 + x^4 + x^3 + x^2 + 1

**Log/antilog tables**:
```c
static unsigned char alpha[256];   // alpha[i] = x^i mod proot
static unsigned char aindex[256];  // aindex[alpha[i]] = i
```

Initialized in `RSECC_init()`:

```c
static void RSECC_init(void)
{
    int i, b;
    alpha[0] = 1;
    aindex[0] = 0;  // undefined, but set to 0
    aindex[1] = 0;
    for(i = 1; i < 255; i++) {
        b = alpha[i-1] << 1;
        if(b & 0x100) b ^= proot;  // reduce mod primitive polynomial
        alpha[i] = (unsigned char)b;
        aindex[b] = i;
    }
}
```

### Generator Polynomial Cache

Generator polynomials are cached for ECC lengths 2 through 30:

```c
static int generator_initialized[29] = {0};
static unsigned char generator[29][31];    // generator[el-2] stores the polynomial
```

`generator_init()` builds the polynomial (x + α^0)(x + α^1)...(x + α^(el-1)):

```c
static void generator_init(int el)
{
    int i, j;
    unsigned char *g = generator[el - 2];
    g[0] = 1;
    for(i = 0; i < el; i++) {
        g[i+1] = 1;
        for(j = i; j > 0; j--) {
            if(g[j] != 0) {
                g[j] = g[j-1] ^ alpha[(aindex[g[j]] + i) % 255];
            } else {
                g[j] = g[j-1];
            }
        }
        g[0] = alpha[(aindex[g[0]] + i) % 255];
    }
}
```

### The Encode Function

```c
int RSECC_encode(int data_length, int ecc_length,
                 const unsigned char *data, unsigned char *ecc)
```

**Thread safety**: Protected by `RSECC_mutex` (pthread mutex) during initialization:

```c
#ifdef HAVE_LIBPTHREAD
static pthread_mutex_t RSECC_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

int RSECC_encode(int data_length, int ecc_length,
                 const unsigned char *data, unsigned char *ecc)
{
    // ... mutex lock ...
    if(!generator_initialized[ecc_length - 2]) {
        if(!initialized) RSECC_init();
        generator_init(ecc_length);
        generator_initialized[ecc_length - 2] = 1;
    }
    // ... mutex unlock ...
```

**Encoding algorithm** — polynomial division:

```c
    unsigned char *gen = generator[ecc_length - 2];
    unsigned char feedback;

    memset(ecc, 0, ecc_length);
    for(i = 0; i < data_length; i++) {
        feedback = aindex[data[i] ^ ecc[0]];
        if(feedback != 255) {
            for(j = 1; j < ecc_length; j++) {
                ecc[j] ^= alpha[(feedback + aindex[gen[ecc_length - j]]) % 255];
            }
        }
        memmove(&ecc[0], &ecc[1], ecc_length - 1);
        if(feedback != 255) {
            ecc[ecc_length - 1] = alpha[(feedback + aindex[gen[0]]) % 255];
        } else {
            ecc[ecc_length - 1] = 0;
        }
    }
```

This implements systematic Reed-Solomon encoding: the `ecc` output is the remainder of dividing the data polynomial by the generator polynomial over GF(2^8).

---

## Format Information

Format information encodes the EC level and mask pattern, protected by BCH(15,5).

### Full QR Format Info

From `qrspec.c`, pre-computed table:

```c
static const unsigned int formatInfo[4][8] = {
    {0x77c4, 0x72f3, 0x7daa, 0x789d, 0x662f, 0x6318, 0x6c41, 0x6976},
    {0x5412, 0x5125, 0x5e7c, 0x5b4b, 0x45f9, 0x40ce, 0x4f97, 0x4aa0},
    {0x355f, 0x3068, 0x3f31, 0x3a06, 0x24b4, 0x2183, 0x2eda, 0x2bed},
    {0x1689, 0x13be, 0x1ce7, 0x19d0, 0x0762, 0x0255, 0x0d0c, 0x083b}
};
```

Indexed by `formatInfo[ec_level][mask_pattern]`. The 15-bit value is written into two locations in the symbol by `Mask_writeFormatInformation()`.

### Micro QR Format Info

From `mqrspec.c`:

```c
static const unsigned int typeTable[MQRSPEC_VERSION_MAX + 1][3] = {
    {0, 0, 0},
    {0x4445, 0x4172, 0x4e2b},
    {0x2f7f, 0x2a48, 0x2511},
    // ...
};
```

---

## How ECC Integrates Into Encoding

The full encoding pipeline in `QRcode_encodeMask()`:

1. **Build byte stream**: `QRinput_getByteStream(input)` → packed data bytes
2. **Create QRRawCode**: `QRraw_new(input)` → initializes RS blocks, runs `RSECC_encode()` on each block
3. **Interleave**: `QRraw_getCode()` called repeatedly to get interleaved data+ECC bytes
4. **Place in frame**: `FrameFiller_next()` places each codeword bit in zigzag order
5. **Apply mask**: One of 8 mask patterns XORed with data area
6. **Format info**: `Mask_writeFormatInformation()` embeds EC level + mask pattern

For Micro QR, `QRcode_encodeMaskMQR()` follows the same pattern but uses `MQRraw_new()`, `MQRraw_getCode()`, and `MMask_*` functions.

---

## RS Block Count Examples

| Version | Level | Group 1 Blocks | Group 1 Data | Group 2 Blocks | Group 2 Data | ECC/Block |
|---|---|---|---|---|---|---|
| 1 | L | 1 | 19 | 0 | — | 7 |
| 1 | H | 1 | 9 | 0 | — | 17 |
| 5 | M | 2 | 43 | 0 | — | 24 |
| 10 | Q | 6 | 24 | 2 | 25 | 26 |
| 40 | L | 19 | 118 | 6 | 119 | 30 |
| 40 | H | 20 | 15 | 61 | 16 | 30 |

The maximum ECC codewords per block is 30, which corresponds to a degree-30 generator polynomial — the maximum cached in `generator[29]`.
