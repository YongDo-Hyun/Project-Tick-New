# genqrcode / libqrencode — Micro QR Code Support

## Overview

Micro QR Code is a compact variant of QR Code standardized in JIS X0510:2004 and ISO/IEC 18004. libqrencode supports Micro QR versions M1 through M4 via dedicated spec tables in `mqrspec.c`, masking in `mmask.c`, and encoding paths in `qrencode.c`.

---

## Micro QR vs. Full QR

| Feature | Full QR Code | Micro QR Code |
|---|---|---|
| Versions | 1–40 | M1–M4 (1–4 internally) |
| Max constant | `QRSPEC_VERSION_MAX = 40` | `MQRSPEC_VERSION_MAX = 4` |
| Finder patterns | 3 | 1 |
| Alignment patterns | 0–46 per version | None |
| Timing patterns | Horizontal + Vertical | Horizontal + Vertical |
| Version information | Versions 7+ | Never |
| Mask patterns | 8 | 4 |
| Mask selection | Minimize penalty | Maximize edge score |
| EC Levels | L, M, Q, H | Version-dependent subset |
| Structured append | Supported | Not supported |
| ECI mode | Supported | Not supported |
| FNC1 mode | Supported | Not supported |

---

## Version Capacities

From `mqrspec.c`:

```c
typedef struct {
    int width;
    int ec[4];
} MQRspec_Capacity;

static const MQRspec_Capacity mqrspecCapacity[MQRSPEC_VERSION_MAX + 1] = {
    {0,  { 0,  0,  0,  0}},
    {11, { 2,  0,  0,  0}},   // M1: 11×11
    {13, { 5,  4,  0,  0}},   // M2: 13×13
    {15, {11,  9,  7,  0}},   // M3: 15×15
    {17, {16, 14, 10,  0}}    // M4: 17×17
};
```

Width formula: `version * 2 + 9` (vs. `version * 4 + 17` for full QR).

### Detailed Capacities

| Version | Width | Total Data Words | L Data | M Data | Q Data | H Data |
|---|---|---|---|---|---|---|
| M1 | 11 | 5 | 2* | — | — | — |
| M2 | 13 | 10 | 5 | 4 | — | — |
| M3 | 15 | 17 | 11 | 9 | 7 | — |
| M4 | 17 | 24 | 16 | 14 | 10 | — |

*M1 has error detection only, not error correction.

### ECC Lengths

```c
int MQRspec_getECCLength(int version, QRecLevel level)
{
    return mqrspecCapacity[version].width * mqrspecCapacity[version].width
           - mqrspecCapacity[version].ec[level]
           - ... (function pattern modules) ...;
}
```

Returns 0 for unsupported version/level combinations, which `QRinput_newMQR()` uses to reject invalid inputs.

### Character Capacities by Mode

| Version | Level | Numeric | Alphanumeric | Byte | Kanji |
|---|---|---|---|---|---|
| M1 | — | 5 | — | — | — |
| M2 | L | 10 | 6 | — | — |
| M2 | M | 8 | 5 | — | — |
| M3 | L | 23 | 14 | 9 | 6 |
| M3 | M | 18 | 11 | 7 | 4 |
| M4 | L | 35 | 21 | 15 | 9 |
| M4 | M | 30 | 18 | 13 | 8 |
| M4 | Q | 21 | 12 | 9 | 5 |

---

## Mode Restrictions

Not all encoding modes are available in every Micro QR version:

| Version | Numeric | Alphanumeric | 8-bit | Kanji | ECI | FNC1 | Structured |
|---|---|---|---|---|---|---|---|
| M1 | Yes | No | No | No | No | No | No |
| M2 | Yes | Yes | No | No | No | No | No |
| M3 | Yes | Yes | Yes | Yes | No | No | No |
| M4 | Yes | Yes | Yes | Yes | No | No | No |

This is enforced by `QRinput_encodeBitStream()` which calls validation functions:

```c
static int QRinput_isModeNumValid(int version, QRinput_List *entry, int mqr)
{
    if(mqr) {
        if(MQRspec_maximumWords(QR_MODE_NUM, version) < entry->size)
            return -1;
    }
    return 0;
}
```

`MQRspec_maximumWords()` returns 0 for unsupported modes at a given version.

---

## MQR-Specific Encoding

### Mode Indicator Sizes

Micro QR uses shorter mode indicators than full QR. From `mqrspec.c`:

```c
// MQR mode indicator bit lengths per version:
// M1: 0 bits (only numeric, implied)
// M2: 1 bit
// M3: 2 bits
// M4: 3 bits
```

These are retrieved by `MQRspec_lengthIndicator()`.

### Character Count Indicator Sizes

```c
static const int lengthTableBits[4][4] = {
    { 3, 0, 0, 0},   // QR_MODE_NUM: M1=3, M2=4, M3=5, M4=6
    { 0, 3, 0, 0},   // QR_MODE_AN:  M1=0, M2=3, M3=4, M4=5
    { 0, 0, 4, 0},   // QR_MODE_8:   M1=0, M2=0, M3=4, M4=5
    { 0, 0, 3, 0},   // QR_MODE_KANJI: M1=0, M2=0, M3=3, M4=4
};
```

A value of 0 means the mode is unsupported for that version.

### Data Length in Bits

A key difference: `MQRspec_getDataLengthBit()` returns data length in **bits**, not bytes:

```c
int MQRspec_getDataLengthBit(int version, QRecLevel level)
{
    int w = mqrspecCapacity[version].width - 1;
    return w * w - 64 - MQRspec_getECCLength(version, level) * 8;
}
```

This matters because M1 and some M2 configurations have data lengths that are not byte-aligned.

---

## MQRRawCode — Micro QR Block Structure

From `qrencode.c`:

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
    int oddbits;       // Number of "odd" bits in last data byte
} MQRRawCode;
```

Micro QR always has exactly **one RS block** (no block interleaving):

```c
static MQRRawCode *MQRraw_new(QRinput *input)
{
    MQRRawCode *raw;
    raw->version = input->version;
    raw->dataLength = MQRspec_getDataLength(input->version, input->level);
    raw->eccLength = MQRspec_getECCLength(input->version, input->level);
    raw->oddbits = raw->dataLength * 8 - MQRspec_getDataLengthBit(input->version, input->level);

    raw->datacode = QRinput_getByteStream(input);
    raw->ecccode = (unsigned char *)malloc(raw->eccLength);
    raw->rsblock_num = 1;
    raw->rsblock = calloc(1, sizeof(RSblock));

    RSblock_initBlock(raw->rsblock, raw->dataLength, raw->datacode,
                      raw->eccLength, raw->ecccode, RSECC_encode);
    raw->count = 0;
    return raw;
}
```

### Odd Bits Handling

The `oddbits` field handles versions where data capacity is not byte-aligned:

```c
unsigned char MQRraw_getCode(MQRRawCode *raw)
{
    if(raw->count < raw->dataLength) {
        return raw->datacode[raw->count++];
    } else {
        return raw->ecccode[raw->count++ - raw->dataLength];
    }
}
```

In `QRcode_encodeMaskMQR()`, the odd bits are handled after placing full codewords:

```c
j = MQRspec_getDataLengthBit(input->version, input->level)
    - MQRraw_getDataLength(raw) * 8;
if(j > 0) {
    // Place remaining odd bits from last data byte
    code = MQRraw_getCode(raw);
    bit = 0x80;
    for(i = 0; i < j; i++) {
        p = FrameFiller_next(filler);
        *p = 0x02 | ((code & bit) != 0);
        bit >>= 1;
    }
}
```

---

## Frame Creation

`MQRspec_createFrame()` builds the base frame with function patterns:

```c
unsigned char *MQRspec_createFrame(int version)
{
    unsigned char *frame;
    int width = mqrspecCapacity[version].width;

    frame = (unsigned char *)calloc(width * width, 1);

    // 1. Finder pattern (only ONE, top-left)
    putFinderPattern(frame, width, 0, 0);

    // 2. Separator (no full separator ring — only right and bottom)

    // 3. Timing pattern (horizontal and vertical)
    for(int i = 0; i < width - 8; i++) {
        // horizontal timing along row 0, starting at column 8
        frame[8 + i] = 0x90 | (i & 1);
        // vertical timing along column 0, starting at row 8
        frame[(8 + i) * width] = 0x90 | (i & 1);
    }

    // 4. Format information area (reserved)
    // No version information (unlike QR versions 7+)
    // No alignment patterns (unlike QR versions 2+)

    return frame;
}
```

Key differences from `QRspec_createFrame()`:
- Single finder pattern instead of three
- No alignment patterns
- No version information area
- Simpler separator structure

---

## Micro QR Masking

See [masking-algorithms.md](masking-algorithms.md) for the full details. Summary:

### 4 Patterns

```c
MMask_mask0: y % 2 == 0
MMask_mask1: (y/2 + x/3) % 2 == 0
MMask_mask2: ((y*x)%2 + (y*x)%3) % 2 == 0
MMask_mask3: ((y+x)%2 + (y*x)%3) % 2 == 0
```

### Selection Criterion

Micro QR picks the mask with the **highest** score (opposite of full QR):

```c
// Sum of dark modules in bottom row × 16 + sum of dark modules in right column
score = sum1 * 16 + sum2;
```

The right column gets lower weight (×1) than the bottom row (×16).

---

## Format Information

Micro QR encodes version, EC level, and mask pattern in a single 15-bit format info:

```c
static const unsigned int typeTable[MQRSPEC_VERSION_MAX + 1][3] = {
    {0x00000, 0x00000, 0x00000},  // unused
    {0x04445, 0x04172, 0x04e2b},  // M1
    {0x02f7f, 0x02a48, 0x02511},  // M2
    {0x07f46, 0x07a71, 0x07528},  // M3
    {0x00dc5, 0x008f2, 0x007ab}   // M4
};
```

Indexed as `typeTable[version][typeNumber]` where `typeNumber` depends on the EC level:

```c
unsigned int MQRspec_getFormatInfo(int mask, int version, QRecLevel level)
{
    // ... compute typeNumber from version and level ...
    // ... XOR with mask-dependent pattern ...
}
```

Written into the symbol by `MMask_writeFormatInformation()` in a single strip around the finder pattern (8 bits on the left side, 7 bits on the top).

---

## API for Micro QR

### Input Creation

```c
QRinput *input = QRinput_newMQR(int version, QRecLevel level);
```

- `version` must be 1–4 (no auto-detection for manual input)
- Invalid version/level combinations are rejected

### High-Level Encoding

```c
QRcode *QRcode_encodeStringMQR(const char *string, int version,
                                QRecLevel level, QRencodeMode hint,
                                int casesensitive);
QRcode *QRcode_encodeString8bitMQR(const char *string, int version,
                                    QRecLevel level);
QRcode *QRcode_encodeDataMQR(int size, const unsigned char *data,
                              int version, QRecLevel level);
```

When `version` is 0, these functions try versions M1 through M4 incrementally:

```c
if(version == 0) {
    for(i = 1; i <= MQRSPEC_VERSION_MAX; i++) {
        QRcode *code = QRcode_encodeDataReal(data, size, i, level, 1);
        if(code != NULL) return code;
    }
}
```

### Version/Level Validation

Use `QRinput_setVersionAndErrorCorrectionLevel()` for Micro QR — it validates the combination. Using `QRinput_setVersion()` or `QRinput_setErrorCorrectionLevel()` individually on MQR inputs returns `EINVAL`.

---

## Structured Append — Not Supported

Micro QR does not support structured append mode. Attempting to encode structured append with MQR results in an error:

```c
static int QRinput_encodeModeStructure(QRinput_List *entry, int mqr)
{
    if(mqr) {
        errno = EINVAL;
        return -1;
    }
    // ...
}
```

`QRinput_Struct_appendInput()` also rejects MQR inputs:

```c
int QRinput_Struct_appendInput(QRinput_Struct *s, QRinput *input)
{
    if(input == NULL || input->mqr) {
        errno = EINVAL;
        return -1;
    }
    // ...
}
```

---

## Encoding Pipeline Differences

`QRcode_encodeMaskMQR()` in `qrencode.c` follows a modified pipeline:

1. **Create frame**: `MQRspec_createFrame(version)` — single finder, no alignment
2. **Initialize RS**: `MQRraw_new(input)` — single block, with odd bits tracking
3. **Place data**: Via `FrameFiller_next()`, but handles odd bits at boundary:
   ```c
   // Place full data bytes
   for(i = 0; i < MQRraw_getDataLength(raw) - 1; i++) {
       code = MQRraw_getCode(raw);
       bit = 0x80;
       for(j = 0; j < 8; j++) {
           p = FrameFiller_next(filler);
           *p = 0x02 | ((bit & code) != 0);
           bit >>= 1;
       }
   }
   // Handle odd bits from last data byte
   ```
4. **Place ECC**: Full ECC bytes, then remainder bits (if any)
5. **Apply mask**: `MMask_mask(version, frame, level)` — 4 patterns, maximize score
6. **Package**: `QRcode_new(version, width, masked)`

---

## CLI Support

The `qrencode` CLI tool supports Micro QR via the `-M` / `--micro` flag:

```bash
qrencode -M -v 3 -l M -o output.png "Hello"
```

In `qrenc.c`:

```c
case 'M':
    micro = 1;
    break;
```

When `micro` is set, `encode()` calls `QRcode_encodeStringMQR()` or `QRcode_encodeDataMQR()` instead of the standard variants.

---

## Limitations Summary

1. **No H level**: Maximum EC is Q (M4 only)
2. **No structured append**: Cannot split data across multiple symbols
3. **No ECI**: Cannot specify character encodings
4. **No FNC1**: Cannot create GS1-compatible codes
5. **Small capacity**: Maximum 35 numeric or 15 byte characters (M4-L)
6. **Single finder**: Only top-left finder pattern — orientation from timing patterns
7. **Version must be specified**: For `QRinput_newMQR()`, version 0 is not auto-detect (though high-level `QRcode_encodeStringMQR()` with version 0 does try all versions)
