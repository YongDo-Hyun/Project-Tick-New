# genqrcode / libqrencode — Encoding Modes

## Mode Indicator Overview

Every QR Code data segment begins with a 4-bit mode indicator (3 bits for Micro QR M1–M3, fewer bits for smaller versions). The library defines modes in the `QRencodeMode` enum:

| Mode | Enum Value | Mode Indicator (QR) | Description |
|---|---|---|---|
| Numeric | `QR_MODE_NUM` (0) | 0001 | Digits 0–9 |
| Alphanumeric | `QR_MODE_AN` (1) | 0010 | 45-character set |
| 8-bit Byte | `QR_MODE_8` (2) | 0100 | Arbitrary byte data |
| Kanji | `QR_MODE_KANJI` (3) | 1000 | Shift-JIS double-byte |
| ECI | `QR_MODE_ECI` (6) | 0111 | Extended Channel Interpretation |
| FNC1 (1st) | `QR_MODE_FNC1FIRST` (7) | 0101 | GS1 first position |
| FNC1 (2nd) | `QR_MODE_FNC1SECOND` (8) | 1001 | GS1 second position |
| Structured | `QR_MODE_STRUCTURE` (4) | 0011 | Internal: structured append |
| Terminator | `QR_MODE_NUL` (-1) | 0000 | Internal: end of data |

---

## Numeric Mode (`QR_MODE_NUM`)

Numeric mode encodes digit characters '0'–'9' at ~3.3 bits per character.

### Encoding Algorithm

From `QRinput_encodeModeNum()` in `qrinput.c`:

```c
static int QRinput_encodeModeNum(QRinput_List *entry, int version, int mqr)
```

1. **Mode indicator**: 4 bits `0001` (or fewer bits for MQR)
2. **Character count indicator**: Variable length from `QRspec_lengthIndicator(QR_MODE_NUM, version)`:
   - Versions 1–9: 10 bits
   - Versions 10–26: 12 bits
   - Versions 27–40: 14 bits
   - MQR M1: 3 bits, M2: 4 bits, M3: 5 bits, M4: 6 bits
3. **Data encoding**: Groups of 3 digits → 10 bits, 2 remaining digits → 7 bits, 1 remaining → 4 bits

The core loop:

```c
for(i = 0; i < words; i++) {
    val  = (entry->data[i*3  ] - '0') * 100;
    val += (entry->data[i*3+1] - '0') * 10;
    val += (entry->data[i*3+2] - '0');
    BitStream_appendNum(entry->bstream, 10, val);
}
if(entry->size - words * 3 == 1) {
    val = entry->data[words*3] - '0';
    BitStream_appendNum(entry->bstream, 4, val);
} else if(entry->size - words * 3 == 2) {
    val  = (entry->data[words*3  ] - '0') * 10;
    val += (entry->data[words*3+1] - '0');
    BitStream_appendNum(entry->bstream, 7, val);
}
```

### Validation

`QRinput_checkModeNum()` verifies every byte is in '0'–'9':

```c
static int QRinput_checkModeNum(int size, const char *data)
{
    int i;
    for(i = 0; i < size; i++) {
        if(data[i] < '0' || data[i] > '9')
            return -1;
    }
    return 0;
}
```

### Bit Cost Estimation

`QRinput_estimateBitsModeNum()`:

```c
int QRinput_estimateBitsModeNum(int size)
{
    int w = size / 3;
    int bits = w * 10;
    switch(size - w * 3) {
        case 1: bits += 4; break;
        case 2: bits += 7; break;
        default: break;
    }
    return bits;
}
```

---

## Alphanumeric Mode (`QR_MODE_AN`)

Encodes 45 characters at ~5.5 bits per character.

### The `QRinput_anTable` Lookup

Defined in `qrinput.c`, this 128-entry table maps ASCII values to the 45-character alphanumeric set:

```c
const signed char QRinput_anTable[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    36, -1, -1, -1, 37, 38, -1, -1, -1, -1, 39, 40, -1, 41, 42, 43,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 44, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
```

The valid characters and their indices:
- `0`–`9` → 0–9
- `A`–`Z` → 10–35
- Space (`' '`) → 36
- `$` → 37, `%` → 38, `*` → 39, `+` → 40
- `-` → 41, `.` → 42, `/` → 43, `:` → 44

### Encoding Algorithm

From `QRinput_encodeModeAn()`:

```c
for(i = 0; i < words; i++) {
    val  = (unsigned int)QRinput_lookAnTable(entry->data[i*2  ]) * 45;
    val += (unsigned int)QRinput_lookAnTable(entry->data[i*2+1]);
    BitStream_appendNum(entry->bstream, 11, val);
}
if(entry->size & 1) {
    val = (unsigned int)QRinput_lookAnTable(entry->data[words*2]);
    BitStream_appendNum(entry->bstream, 6, val);
}
```

- **Pairs** of characters → multiply first by 45, add second → 11-bit value
- **Odd last character** → 6-bit value
- Character count indicator lengths: 9/11/13 bits for version groups 1–9/10–26/27–40

### Validation

`QRinput_checkModeAn()` uses `QRinput_lookAnTable()`:

```c
static int QRinput_checkModeAn(int size, const char *data)
{
    int i;
    for(i = 0; i < size; i++) {
        if(QRinput_lookAnTable(data[i]) < 0)
            return -1;
    }
    return 0;
}
```

Where `QRinput_lookAnTable()` returns -1 for any byte ≥ 128 or any byte mapping to -1 in the table.

---

## 8-bit Byte Mode (`QR_MODE_8`)

Encodes arbitrary bytes at 8 bits per character with no transformation.

### Encoding

From `QRinput_encodeMode8()`:

```c
static int QRinput_encodeMode8(QRinput_List *entry, int version, int mqr)
{
    // ... mode indicator and count ...
    ret = BitStream_appendBytes(entry->bstream, entry->size, entry->data);
    // ...
}
```

Character count indicator lengths: 8/16/16 bits for version groups.

### Validation

No validation — all byte values 0x00–0xFF are accepted:

```c
static int QRinput_checkMode8(int size, const char *data)
{
    (void)size; (void)data;
    return 0;
}
```

---

## Kanji Mode (`QR_MODE_KANJI`)

Encodes Shift-JIS double-byte characters at 13 bits per character.

### Encoding Algorithm

From `QRinput_encodeModeKanji()`:

```c
for(i = 0; i < entry->size; i += 2) {
    val = ((unsigned int)entry->data[i] << 8) | entry->data[i+1];
    if(val <= 0x9ffc) {
        val -= 0x8140;
    } else {
        val -= 0xc140;
    }
    val = (val >> 8) * 0xc0 + (val & 0xff);
    BitStream_appendNum(entry->bstream, 13, val);
}
```

**Steps per character:**
1. Combine two bytes into a 16-bit value
2. Subtract `0x8140` (if ≤ 0x9FFC) or `0xC140` (otherwise)
3. Decompose: high byte × 0xC0 + low byte → 13-bit output

Character count indicator lengths: 8/10/12 bits. Count is **number of characters** (bytes / 2).

### Validation

`QRinput_checkModeKanji()` requires even size, and each pair must be in valid Shift-JIS ranges:

```c
static int QRinput_checkModeKanji(int size, const unsigned char *data)
{
    int i;
    unsigned int val;
    if(size & 1) return -1;
    for(i = 0; i < size; i += 2) {
        val = ((unsigned int)data[i] << 8) | data[i+1];
        if(val < 0x8140 || (val > 0x9ffc && val < 0xe040) || val > 0xebbf)
            return -1;
    }
    return 0;
}
```

---

## ECI Mode (`QR_MODE_ECI`)

Extended Channel Interpretation selects a character encoding for subsequent data.

### Encoding

From `QRinput_encodeModeECI()`:

```c
static int QRinput_encodeModeECI(QRinput_List *entry, int version)
{
    // ... mode indicator 0111 ...
    unsigned int ecinum = (entry->data[0])
                        | (entry->data[1] << 8)
                        | (entry->data[2] << 16)
                        | (entry->data[3] << 24);
    if(ecinum < 128) {
        BitStream_appendNum(entry->bstream, 8, ecinum);
    } else if(ecinum < 16384) {
        BitStream_appendNum(entry->bstream, 2, 2);     // 10
        BitStream_appendNum(entry->bstream, 14, ecinum);
    } else {
        BitStream_appendNum(entry->bstream, 3, 6);     // 110
        BitStream_appendNum(entry->bstream, 21, ecinum);
    }
    return 0;
}
```

ECI number stored as 4-byte little-endian internally. The encoding uses variable-length:
- 0–127: 8 bits (0xxxxxxx)
- 128–16383: 16 bits (10xxxxxxxxxxxxxx)
- 16384–999999: 24 bits (110xxxxxxxxxxxxxxxxxxxxx)

### Usage

ECI numbers are set via `QRinput_appendECIheader()`, not `QRinput_append()`. The function validates `ecinum ≤ 999999` and stores it in little-endian:

```c
int QRinput_appendECIheader(QRinput *input, unsigned int ecinum)
{
    unsigned char data[4];
    if(ecinum > 999999) { errno = EINVAL; return -1; }
    data[0] = ecinum & 0xff;
    data[1] = (ecinum >>  8) & 0xff;
    data[2] = (ecinum >> 16) & 0xff;
    data[3] = (ecinum >> 24) & 0xff;
    return QRinput_append(input, QR_MODE_ECI, 4, data);
}
```

Common ECI assignments:
- 3: ISO/IEC 8859-1 (Latin-1)
- 20: Shift JIS
- 26: UTF-8

---

## FNC1 Mode

### First Position (`QR_MODE_FNC1FIRST`)

Used for GS1 QR Codes. Sets  mode indicator `0101`.

Encoded via `QRinput_encodeModeFNC1First()`:

```c
static int QRinput_encodeModeFNC1First(QRinput_List *entry, int version)
{
    // Just the mode indicator, no data follows
    BitStream_appendNum(entry->bstream, 4, MODEID_FNC1FIRST);
    return 0;
}
```

Set via `QRinput_setFNC1First()`.

### Second Position (`QR_MODE_FNC1SECOND`)

Mode indicator `1001`, followed by 1-byte application identifier.

```c
static int QRinput_encodeModeFNC1Second(QRinput_List *entry, int version)
{
    BitStream_appendNum(entry->bstream, 4, MODEID_FNC1SECOND);
    BitStream_appendBytes(entry->bstream, 1, entry->data);
    return 0;
}
```

Validation: `QRinput_checkModeFNC1Second()` requires `size == 1`.

---

## Structured Append Mode (`QR_MODE_STRUCTURE`)

Internal mode for linking multiple QR Code symbols. Not set directly by users.

```c
static int QRinput_encodeModeStructure(QRinput_List *entry, int mqr)
{
    if(mqr) { errno = EINVAL; return -1; }  // not supported in MQR
    BitStream_appendNum(entry->bstream, 4, MODEID_STRUCTURE);
    BitStream_appendNum(entry->bstream, 4, entry->data[1] - 1);  // total symbols - 1
    BitStream_appendNum(entry->bstream, 4, entry->data[0] - 1);  // current position - 1
    BitStream_appendBytes(entry->bstream, 1, &entry->data[2]);   // parity byte
    return 0;
}
```

Total overhead: 4 + 4 + 4 + 8 = 20 bits per symbol.

---

## Mode Selection — The Split Algorithm

When using high-level functions like `QRcode_encodeString()`, the library automatically selects optimal encoding modes via `Split_splitStringToQRinput()` in `split.c`.

### Mode Identification

`Split_identifyMode()` classifies each byte:

```c
static QRencodeMode Split_identifyMode(const char *string, QRencodeMode hint)
{
    unsigned char c = string[0];

    if(isdigit(c)) {
        return QR_MODE_NUM;
    } else if(QRinput_lookAnTable(c) >= 0) {
        return QR_MODE_AN;
    } else if(hint == QR_MODE_KANJI) {
        if(iskanji(c, string[1])) {
            return QR_MODE_KANJI;
        }
    }
    return QR_MODE_8;
}
```

### The Eat Functions

The splitter uses four "eat" functions that greedily consume characters:

**`Split_eatNum()`**: Starts in numeric mode, looks ahead to decide:
- If next non-digit is alphanumeric, calculates bit cost difference:
  ```c
  dif = QRinput_estimateBitsModeNum(run)
      + QRinput_estimateBitsModeAn(1)
      - QRinput_estimateBitsModeAn(run + 1);
  if(dif > 0) {
      // switch to AN mode — it's cheaper
  }
  ```
- If next non-digit is 8-bit, compares numeric-then-8-bit cost vs. all-8-bit:
  ```c
  dif = QRinput_estimateBitsModeNum(run)
      + 4 + ln   // mode switch overhead
      - QRinput_estimatebitsModeMode8(run);
  if(dif > 0) {
      // encode remaining as 8-bit
  }
  ```

**`Split_eatAn()`**: Consumes alphanumeric characters, decides whether to switch to numeric mode or 8-bit mode based on bit efficiency.

**`Split_eatKanji()`**: Consumes consecutive valid Shift-JIS Kanji pairs.

**`Split_eat8()`**: Consumes 8-bit bytes, checks for opportunities to switch to numeric, alphanumeric, or Kanji by looking at upcoming runs.

### Case Sensitivity

When `casesensitive == 0`, `Split_splitStringToQRinput()` calls `dupAndToUpper()`:

```c
static char *dupAndToUpper(const char *str, QRencodeMode hint)
{
    char *newstr, *p;
    newstr = strdup(str);
    if(hint == QR_MODE_KANJI) return newstr;  // skip for Kanji
    p = newstr;
    while(*p) {
        if(*p >= 'a' && *p <= 'z') *p = (char)((int)*p - 32);
        p++;
    }
    return newstr;
}
```

Converting to uppercase allows more characters to use the more efficient alphanumeric mode.

---

## Mode Length Indicator Sizes

The bit width of the character count indicator varies by version. Defined in `qrspec.c`:

```c
static const int lengthTableBits[4][3] = {
    {10, 12, 14},   // QR_MODE_NUM
    { 9, 11, 13},   // QR_MODE_AN
    { 8, 16, 16},   // QR_MODE_8
    { 8, 10, 12}    // QR_MODE_KANJI
};
```

Version groups: 1–9, 10–26, 27–40.

For Micro QR, lengths are defined in `mqrspec.c` via `MQRspec_lengthTableBits[4][4]`:

| Mode | M1 | M2 | M3 | M4 |
|---|---|---|---|---|
| NUM | 3 | 4 | 5 | 6 |
| AN | 0 | 3 | 4 | 5 |
| 8 | 0 | 0 | 4 | 5 |
| KANJI | 0 | 0 | 3 | 4 |

A zero means the mode is not available for that version. This is checked by `QRinput_isModeNumValid()` and siblings, which call `MQRspec_maximumWords()`.

---

## Bit Estimation Functions

Used internally for mode optimization and version selection.

### Per-Mode Estimators

```c
int QRinput_estimateBitsModeNum(int size);   // (size/3)*10 + [4|7|0]
int QRinput_estimateBitsModeAn(int size);    // (size/2)*11 + [6|0]
int QRinput_estimateBitsMode8(int size);     // size * 8
int QRinput_estimateBitsModeKanji(int size); // (size/2) * 13
```

### Total Stream Estimation

`QRinput_estimateBitStreamSize()` sums across all entries:

```c
static int QRinput_estimateBitStreamSize(QRinput *input, int version)
{
    QRinput_List *list = input->head;
    int bits = 0;
    while(list != NULL) {
        bits += QRinput_estimateBitStreamSizeOfEntry(list, version, input->mqr);
        list = list->next;
    }
    return bits;
}
```

Each entry contribution = mode indicator bits + count indicator bits + data bits.

### Version Auto-Selection

`QRinput_estimateVersion()` iterates to convergence:

```c
static int QRinput_estimateVersion(QRinput *input)
{
    int bits, version, prev;
    version = 0;
    do {
        prev = version;
        bits = QRinput_estimateBitStreamSize(input, prev);
        version = QRspec_getMinimumVersion((bits + 7) / 8, input->level);
        if(version < 0) return -1;  // ERANGE
    } while(version > prev);
    return version;
}
```

The version may increase because larger versions have longer count indicators, which in turn require more bits. The loop converges because version increases monotonically and is bounded by 40.

---

## Micro QR Mode Restrictions

Not all modes are available in all Micro QR versions:

| Version | Available Modes |
|---|---|
| M1 | Numeric only |
| M2 | Numeric, Alphanumeric |
| M3 | Numeric, Alphanumeric, 8-bit, Kanji |
| M4 | Numeric, Alphanumeric, 8-bit, Kanji |

Enforced by `QRinput_isModeNumValid()`, `QRinput_isModeAnValid()`, `QRinput_isMode8Valid()`, `QRinput_isModeKanjiValid()`. Each calls `MQRspec_maximumWords(version, mode)` and returns error if the result is 0.

---

## BitStream: Internal Encoding Engine

All mode encoders produce output through the `BitStream` type defined in `bitstream.h`:

```c
typedef struct {
    size_t length;    // current number of bits stored
    size_t datasize;  // allocated capacity in bytes (1 bit per byte!)
    unsigned char *data;
} BitStream;
```

**Storage**: Each bit occupies one byte (values 0 or 1). This wastes memory but simplifies manipulation.

**Core operations:**
- `BitStream_appendNum(bstream, bits, val)` — Appends `bits` bits from integer `val`
- `BitStream_appendBytes(bstream, size, data)` — Appends `size` bytes as `size * 8` bits
- `BitStream_append(bstream, src)` — Concatenates two bit streams
- `BitStream_toByte(bstream)` — Packs 1-bit-per-byte into 8-bits-per-byte format

**Growth**: `DEFAULT_BUFSIZE = 128`, doubles on overflow via `BitStream_allocate()`.

The final `QRinput_getByteStream()` function calls `QRinput_getBitStream()` (which chains all entry bit streams with padding) and then `BitStream_toByte()` to produce the packed byte array consumed by the RS encoder.

---

## Padding Algorithm

After all data entries are encoded, `QRinput_createBitStream()` calls `QRinput_createPaddingBit()` to fill remaining capacity:

```c
static int QRinput_createPaddingBit(QRinput *input)
{
    // ... calculate remaining capacity ...

    // Add terminator (0000): up to 4 bits for QR, variable for MQR
    if(bits > terminator) {
        bits -= terminator;
    } else {
        // terminator alone fills remaining space
    }

    // Align to byte boundary
    padlen = 8 - (QRinput_lengthOfCode(input) * 8 + input->mqr) % 8;
    if(padlen == 8) padlen = 0;

    // Fill with alternating 0xEC, 0x11
    padlen = maxwords - (QRinput_lengthOfCode(input));
    for(i = 0; i < padlen; i++) {
        padbuf[i] = (i & 1) ? 0x11 : 0xEC;
    }
}
```

Padding bytes `0xEC` and `0x11` are specified by the QR Code standard.
