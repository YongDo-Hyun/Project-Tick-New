# genqrcode / libqrencode — Overview

## Introduction

genqrcode is Project-Tick's integrated copy of **libqrencode**, a fast and compact C library for encoding data into QR Code symbols. Originally developed by Kentaro Fukuchi and distributed under the GNU Lesser General Public License v2.1+, the library implements QR Code Model 2 as specified in **JIS X0510:2004** and **ISO/IEC 18004:2006**.

The library encodes input data into a raw bitmap array (`unsigned char *`) representing the QR Code matrix. Unlike tools that produce image files directly, libqrencode gives applications direct access to the symbol matrix, enabling flexible rendering into any output format. The accompanying `qrencode` CLI tool wraps the library and produces image files in PNG, EPS, SVG, XPM, and various terminal text formats.

The current version integrated in Project-Tick is **4.1.1**.

---

## Feature Summary

### Core Capabilities

| Feature | Description |
|---|---|
| **QR Code Model 2** | Full implementation of the modern QR Code standard |
| **Micro QR Code** | Experimental support for M1–M4 (versions 1–4) |
| **Versions 1–40** | Full-size QR Code from 21×21 to 177×177 modules |
| **Auto-version selection** | Automatically selects minimum version for given data |
| **Structured Append** | Split large data across up to 16 linked QR symbols |
| **Optimized encoding** | Automatic input parsing selects optimal encoding modes |
| **Thread-safe** | Optional pthread mutex protection for concurrent use |

### Encoding Modes

The library supports all standard encoding modes defined in the QR Code specification:

| Mode | Enum Value | Bit Indicator | Characters |
|---|---|---|---|
| **Numeric** | `QR_MODE_NUM` (0) | `0001` | Digits 0–9 |
| **Alphanumeric** | `QR_MODE_AN` (1) | `0010` | 0–9, A–Z, space, $, %, *, +, -, ., /, : |
| **8-bit Byte** | `QR_MODE_8` (2) | `0100` | Any 8-bit byte (ISO 8859-1 / UTF-8) |
| **Kanji** | `QR_MODE_KANJI` (3) | `1000` | Shift-JIS double-byte characters |
| **ECI** | `QR_MODE_ECI` (6) | `0111` | Extended Channel Interpretation headers |
| **FNC1 (1st pos)** | `QR_MODE_FNC1FIRST` (7) | `0101` | GS1 DataBar compatibility |
| **FNC1 (2nd pos)** | `QR_MODE_FNC1SECOND` (8) | `1001` | Application identifier mode |

Internal-only modes:

| Mode | Enum Value | Purpose |
|---|---|---|
| `QR_MODE_NUL` | -1 | Terminator sentinel |
| `QR_MODE_STRUCTURE` | 5 | Structured append header |

These are defined as the `QRencodeMode` enum in `qrencode.h`.

### Error Correction Levels

Four Reed-Solomon error correction levels are supported, defined as the `QRecLevel` enum:

| Level | Enum | Recovery Capability | Typical Use |
|---|---|---|---|
| **L** | `QR_ECLEVEL_L` (0) | ~7% codewords | Maximum data capacity |
| **M** | `QR_ECLEVEL_M` (1) | ~15% codewords | Standard use |
| **Q** | `QR_ECLEVEL_Q` (2) | ~25% codewords | Higher reliability |
| **H** | `QR_ECLEVEL_H` (3) | ~30% codewords | Maximum error recovery |

### QR Code Versions and Capacity

QR Code versions range from 1 to 40, each adding 4 modules per side. The maximum version constant is `QRSPEC_VERSION_MAX` (40).

| Version | Size (modules) | Max Data (L) | Max Data (H) |
|---|---|---|---|
| 1 | 21 × 21 | 19 bytes | 9 bytes |
| 5 | 37 × 37 | 108 bytes | 46 bytes |
| 10 | 57 × 57 | 274 bytes | 122 bytes |
| 20 | 97 × 97 | 861 bytes | 385 bytes |
| 30 | 137 × 137 | 1735 bytes | 745 bytes |
| 40 | 177 × 177 | 2956 bytes | 1276 bytes |

The full capacity table is stored in `qrspec.c` as `qrspecCapacity[QRSPEC_VERSION_MAX + 1]`, a static array of `QRspec_Capacity` structures:

```c
typedef struct {
    int width;     // Edge length of the symbol
    int words;     // Data capacity (bytes)
    int remainder; // Remainder bit (bits)
    int ec[4];     // Number of ECC code (bytes) per level
} QRspec_Capacity;
```

### Micro QR Code Versions

Micro QR supports versions M1 through M4 (`MQRSPEC_VERSION_MAX` = 4):

| Version | Size | Max EC | Modes Supported |
|---|---|---|---|
| M1 | 11 × 11 | Error detection only | Numeric only |
| M2 | 13 × 13 | L, M | Numeric, Alphanumeric |
| M3 | 15 × 15 | L, M | Numeric, Alphanumeric, 8-bit, Kanji |
| M4 | 17 × 17 | L, M, Q | Numeric, Alphanumeric, 8-bit, Kanji |

---

## Output Data Format

The encoded QR Code is returned as a `QRcode` struct:

```c
typedef struct {
    int version;         // version of the symbol
    int width;           // width of the symbol
    unsigned char *data; // symbol data
} QRcode;
```

The `data` field is a flat array of `width * width` unsigned chars. Each byte represents one module (dot) with the following bit layout:

```
MSB 76543210 LSB
    |||||||`- 1=black/0=white
    ||||||`-- 1=ecc/0=data code area
    |||||`--- format information
    ||||`---- version information
    |||`----- timing pattern
    ||`------ alignment pattern
    |`------- finder pattern and separator
    `-------- non-data modules (format, timing, etc.)
```

For most applications, only the least significant bit (bit 0) matters — it determines whether a module is black (1) or white (0). The higher bits provide metadata about what type of QR Code element occupies that position.

### Rendering Example

From `qrencode.h`:

```c
QRcode *qrcode;
qrcode = QRcode_encodeString("TEST", 0, QR_ECLEVEL_M, QR_MODE_8, 1);
if(qrcode == NULL) abort();

for(int y = 0; y < qrcode->width; y++) {
    for(int x = 0; x < qrcode->width; x++) {
        if(qrcode->data[y * qrcode->width + x] & 1) {
            draw_black_dot(x, y);
        } else {
            draw_white_dot(x, y);
        }
    }
}
QRcode_free(qrcode);
```

---

## API Surface Overview

The public API is declared in `qrencode.h` and falls into these categories:

### Input Construction

| Function | Purpose |
|---|---|
| `QRinput_new()` | Create input object (version=0/auto, level=L) |
| `QRinput_new2(version, level)` | Create input with explicit version and level |
| `QRinput_newMQR(version, level)` | Create Micro QR input object |
| `QRinput_append(input, mode, size, data)` | Append data chunk to input |
| `QRinput_appendECIheader(input, ecinum)` | Append ECI header |
| `QRinput_getVersion(input)` | Get current version |
| `QRinput_setVersion(input, version)` | Set version (not for MQR) |
| `QRinput_getErrorCorrectionLevel(input)` | Get current EC level |
| `QRinput_setErrorCorrectionLevel(input, level)` | Set EC level (not for MQR) |
| `QRinput_setVersionAndErrorCorrectionLevel(input, version, level)` | Set both (recommended for MQR) |
| `QRinput_free(input)` | Free input and all chunks |
| `QRinput_check(mode, size, data)` | Validate input data |
| `QRinput_setFNC1First(input)` | Set FNC1 first position flag |
| `QRinput_setFNC1Second(input, appid)` | Set FNC1 second position with app ID |

### Structured Append

| Function | Purpose |
|---|---|
| `QRinput_Struct_new()` | Create structured input set |
| `QRinput_Struct_setParity(s, parity)` | Set parity for structured symbols |
| `QRinput_Struct_appendInput(s, input)` | Append QRinput to set |
| `QRinput_Struct_free(s)` | Free all inputs in set |
| `QRinput_splitQRinputToStruct(input)` | Auto-split input into structured set |
| `QRinput_Struct_insertStructuredAppendHeaders(s)` | Insert SA headers |

### Encoding (Simple API)

| Function | Purpose |
|---|---|
| `QRcode_encodeString(string, version, level, hint, casesensitive)` | Auto-parse and encode string |
| `QRcode_encodeString8bit(string, version, level)` | Encode string as 8-bit |
| `QRcode_encodeData(size, data, version, level)` | Encode raw byte data |
| `QRcode_encodeInput(input)` | Encode from QRinput object |
| `QRcode_free(qrcode)` | Free QRcode result |

### Encoding (Micro QR)

| Function | Purpose |
|---|---|
| `QRcode_encodeStringMQR(...)` | Auto-parse string to Micro QR |
| `QRcode_encodeString8bitMQR(...)` | 8-bit string to Micro QR |
| `QRcode_encodeDataMQR(...)` | Raw data to Micro QR |

### Encoding (Structured Append)

| Function | Purpose |
|---|---|
| `QRcode_encodeInputStructured(s)` | Encode structured input |
| `QRcode_encodeStringStructured(...)` | Auto-split and encode string |
| `QRcode_encodeString8bitStructured(...)` | 8-bit structured encoding |
| `QRcode_encodeDataStructured(...)` | Raw data structured encoding |
| `QRcode_List_size(qrlist)` | Count symbols in list |
| `QRcode_List_free(qrlist)` | Free symbol list |

### Utility

| Function | Purpose |
|---|---|
| `QRcode_APIVersion(&major, &minor, &micro)` | Get version numbers |
| `QRcode_APIVersionString()` | Get version string |
| `QRcode_clearCache()` | Deprecated, no-op |

---

## Source File Inventory

The library consists of the following source files:

### Core Library

| File | Purpose |
|---|---|
| `qrencode.h` | Public API header — all external declarations |
| `qrencode.c` | Core encoding engine — QRRawCode, FrameFiller, QRcode_encode* |
| `qrencode_inner.h` | Internal header for test access to private types |
| `qrinput.h` / `qrinput.c` | Input data management, mode encoding, bit stream construction |
| `bitstream.h` / `bitstream.c` | Binary sequence (bit array) class |
| `qrspec.h` / `qrspec.c` | QR Code spec tables — capacity, ECC, alignment, frame creation |
| `mqrspec.h` / `mqrspec.c` | Micro QR Code spec tables and frame creation |
| `rsecc.h` / `rsecc.c` | Reed-Solomon error correction encoder |
| `split.h` / `split.c` | Input string splitter — automatic mode detection and optimization |
| `mask.h` / `mask.c` | Masking for full QR Code — 8 patterns, penalty evaluation |
| `mmask.h` / `mmask.c` | Masking for Micro QR Code — 4 patterns |

### CLI Tool

| File | Purpose |
|---|---|
| `qrenc.c` | Command-line `qrencode` tool — PNG, EPS, SVG, XPM, ANSI, ASCII, UTF-8 output |

### Build System

| File | Purpose |
|---|---|
| `CMakeLists.txt` | CMake build configuration |
| `configure.ac` | Autotools configure script template |
| `Makefile.am` | Automake makefile template |
| `autogen.sh` | Script to generate `configure` from `configure.ac` |
| `libqrencode.pc.in` | pkg-config template |
| `qrencode.1.in` | Man page template |
| `cmake/FindIconv.cmake` | CMake module for finding iconv |

### Test Suite

Located in `tests/`:

| File | Tests |
|---|---|
| `test_bitstream.c` | BitStream class operations |
| `test_estimatebit.c` | Bit stream size estimation |
| `test_qrinput.c` | Input data handling and encoding |
| `test_qrspec.c` | QR specification tables and frame generation |
| `test_mqrspec.c` | Micro QR specification |
| `test_qrencode.c` | End-to-end encoding |
| `test_split.c` | String splitting and mode optimization |
| `test_split_urls.c` | URL-specific splitting tests |
| `test_mask.c` | Mask pattern and penalty evaluation |
| `test_mmask.c` | Micro QR mask patterns |
| `test_rs.c` | Reed-Solomon encoder correctness |
| `test_monkey.c` | Randomized fuzz testing |
| `prof_qrencode.c` | Performance profiling |
| `pthread_qrencode.c` | Thread safety testing |

---

## Supported Standards

- **JIS X0510:2004** — "Two dimensional symbol — QR-code — Basic Specification"
- **ISO/IEC 18004:2006** — "Automatic identification and data capture techniques — QR Code 2005 bar code symbology specification"

The source code frequently references specific sections and tables from these standards. For example:
- Capacity tables: Table 1 (p.13) and Tables 12-16 (pp.30-36) of JIS X0510:2004
- Mode indicators: Table 2 of JIS X0510:2004 (p.16)
- Penalty rules: Section 8.8.2 (p.45) of JIS X0510:2004
- ECI encoding: Table 4 of JIS X0510:2004 (p.17)
- Alignment patterns: Table 1 in Appendix E (p.71) of JIS X0510:2004
- Version information: Table 1 in Appendix D (p.68) of JIS X0510:2004
- Micro QR format info: Table 10 of Appendix 1 (p.115) of JIS X0510:2004

---

## What Is NOT Supported

The README explicitly lists features not implemented:

- **QR Code Model 1** — The deprecated original model
- **ECI mode** — Listed as unsupported in README, though the code has partial implementation with `QR_MODE_ECI` and `QRinput_appendECIheader()` / `QRinput_encodeModeECI()`
- **FNC1 mode** — Similarly listed as unsupported in README, but has code paths for `QR_MODE_FNC1FIRST` and `QR_MODE_FNC1SECOND`

> **Note:** The code contains working implementations for ECI and FNC1 modes despite the README claiming they are unsupported. The README may be outdated — these modes appear functional based on code analysis.

---

## Thread Safety

When built with pthread support (`--enable-thread-safety` for Autotools, or automatic detection in CMake), the library uses a mutex to protect:

1. **Reed-Solomon initialization** — The GF(2^8) lookup tables and generator polynomials are lazily initialized. A `pthread_mutex_t` in `rsecc.c` guards `RSECC_init()` and `generator_init()`.

The `HAVE_LIBPTHREAD` preprocessor macro controls this behavior. Functions marked as "THREAD UNSAFE when pthread is disabled" in the API documentation include all `QRcode_encode*` functions, as they share global RS state without mutex protection when pthread is not available.

---

## Output Formats (CLI Tool)

The `qrencode` CLI tool supports the following output formats via the `-t` flag:

| Format | Description |
|---|---|
| `PNG` | Indexed-color PNG (1-bit, palette-based) |
| `PNG32` | 32-bit RGBA PNG |
| `EPS` | Encapsulated PostScript |
| `SVG` | Scalable Vector Graphics (with optional RLE and path mode) |
| `XPM` | X PixMap format |
| `ANSI` | ANSI terminal escape codes (16-color) |
| `ANSI256` | ANSI terminal escape codes (256-color) |
| `ASCII` | ASCII art (# for black, space for white) |
| `ASCIIi` | Inverted ASCII art |
| `UTF8` | Unicode block characters |
| `UTF8i` | Inverted UTF-8 |
| `ANSIUTF8` | UTF-8 with ANSI color codes |
| `ANSIUTF8i` | Inverted UTF-8 with ANSI color codes |
| `ANSI256UTF8` | UTF-8 with 256-color ANSI codes |

---

## License

The library is licensed under the **GNU Lesser General Public License v2.1** or any later version. The Reed-Solomon encoder is derived from Phil Karn's (KA9Q) FEC library, also under LGPL.

```
Copyright (C) 2006-2018, 2020 Kentaro Fukuchi <kentaro@fukuchi.org>
Reed-Solomon: Copyright (C) 2002, 2003, 2004, 2006 Phil Karn, KA9Q
```

---

## Key Constants

Defined across the headers:

```c
// qrencode.h
#define QRSPEC_VERSION_MAX 40     // Maximum QR version
#define MQRSPEC_VERSION_MAX 4     // Maximum Micro QR version

// qrspec.h
#define QRSPEC_WIDTH_MAX 177      // Maximum symbol width (version 40)

// mqrspec.h
#define MQRSPEC_WIDTH_MAX 17      // Maximum Micro QR width (M4)

// qrinput.h
#define MODE_INDICATOR_SIZE 4     // Bits for mode indicator
#define STRUCTURE_HEADER_SIZE 20  // Bits for structured append header
#define MAX_STRUCTURED_SYMBOLS 16 // Max symbols in structured set

// qrspec.h — Mode indicator values
#define QRSPEC_MODEID_ECI        7
#define QRSPEC_MODEID_NUM        1
#define QRSPEC_MODEID_AN         2
#define QRSPEC_MODEID_8          4
#define QRSPEC_MODEID_KANJI      8
#define QRSPEC_MODEID_FNC1FIRST  5
#define QRSPEC_MODEID_FNC1SECOND 9
#define QRSPEC_MODEID_STRUCTURE  3
#define QRSPEC_MODEID_TERMINATOR 0

// mqrspec.h — Micro QR mode indicator values
#define MQRSPEC_MODEID_NUM       0
#define MQRSPEC_MODEID_AN        1
#define MQRSPEC_MODEID_8         2
#define MQRSPEC_MODEID_KANJI     3
```

---

## Dependencies

The library itself has **no external dependencies**. Optional dependencies are:

| Dependency | Required For | Detection |
|---|---|---|
| **libpng** | PNG output in CLI tool | pkg-config / CMake `find_package` |
| **SDL 2.0** | `view_qrcode` test viewer | pkg-config |
| **libiconv** | Decoder in test suite | CMake `find_package` / `AM_ICONV_LINK` |
| **pthreads** | Thread safety | `AC_CHECK_LIB` / CMake `find_package(Threads)` |

---

## Building

The library supports two build systems:

1. **Autotools** — `./configure && make && make install`
2. **CMake** — `cmake . && make`

Both produce:
- `libqrencode.{a,so,dylib}` — The library
- `qrencode` — The CLI tool (optional)
- `libqrencode.pc` — pkg-config file
- `qrencode.1` — Man page

See [building.md](building.md) for detailed build instructions.

---

## Structured Append

Structured Append allows splitting a large data set across multiple QR Code symbols (up to `MAX_STRUCTURED_SYMBOLS` = 16). Each symbol carries a header encoding:

- Total number of symbols (4 bits)
- Symbol index (4 bits)  
- Parity byte (8 bits)

Total header overhead: 20 bits (`STRUCTURE_HEADER_SIZE`) per symbol.

The library provides both automatic splitting (`QRinput_splitQRinputToStruct()`, `QRcode_encodeStringStructured()`) and manual construction (`QRinput_Struct_new()`, `QRinput_Struct_appendInput()`).

Example from the public API documentation:

```c
QRcode_List *qrcodes;
QRcode_List *entry;
QRcode *qrcode;

qrcodes = QRcode_encodeStringStructured(...);
entry = qrcodes;
while(entry != NULL) {
    qrcode = entry->code;
    // render qrcode
    entry = entry->next;
}
QRcode_List_free(entry);
```

---

## Version Auto-Selection

When version is set to 0 (the default for `QRinput_new()`), the library automatically selects the minimum version that can accommodate the input data. This is implemented in `QRinput_estimateVersion()` in `qrinput.c`:

```c
STATIC_IN_RELEASE int QRinput_estimateVersion(QRinput *input)
{
    int bits;
    int version, prev;

    version = 0;
    do {
        prev = version;
        bits = QRinput_estimateBitStreamSize(input, prev);
        version = QRspec_getMinimumVersion((bits + 7) / 8, input->level);
        if(prev == 0 && version > 1) {
            version--;
        }
    } while (version > prev);

    return version;
}
```

This iterates because changing the version changes the length indicator sizes, which in turn affects the total bit count. The loop converges when the estimated version matches the previous iteration.

For Micro QR encoding (`QRcode_encodeStringMQR`, `QRcode_encodeDataMQR`), auto-selection works by trying each version from the specified minimum up to `MQRSPEC_VERSION_MAX` (4) until encoding succeeds.

---

## Encoding Pipeline Summary

The high-level data flow from input string to QR Code matrix is:

1. **Input parsing** — `Split_splitStringToQRinput()` analyzes the input and splits it into optimal mode segments (numeric, alphanumeric, 8-bit, Kanji)
2. **Bit stream construction** — Each segment is encoded according to its mode and appended to a `BitStream`
3. **Version estimation** — The minimum version is selected based on total bit count and error correction level
4. **Padding** — Terminator pattern and pad codewords (`0xEC`, `0x11` alternating) are appended
5. **RS encoding** — Reed-Solomon ECC codewords are computed for each data block via `RSECC_encode()`
6. **Interleaving** — Data and ECC blocks are interleaved according to the QR spec
7. **Frame creation** — `QRspec_newFrame()` builds the base frame with finder patterns, timing patterns, alignment patterns, and version information
8. **Module placement** — `FrameFiller_next()` places data/ECC bits into the frame in the correct zigzag pattern
9. **Masking** — All 8 mask patterns are applied and evaluated; the one with the lowest penalty score is selected
10. **Format information** — BCH-encoded format info (EC level + mask) is written into the frame
11. **Output** — The completed frame is returned as a `QRcode` struct

See [architecture.md](architecture.md) for detailed module relationships and data flow.
