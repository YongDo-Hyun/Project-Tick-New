# genqrcode / libqrencode — Public API Reference

## Header

All public API declarations are in `qrencode.h`. This is the only header that consumers need to include:

```c
#include <qrencode.h>
```

The header is guarded by `QRENCODE_H` and includes `extern "C"` linkage for C++ compatibility.

---

## Types

### QRencodeMode

Encoding mode enumeration:

```c
typedef enum {
    QR_MODE_NUL = -1,    // Terminator (internal use only)
    QR_MODE_NUM = 0,     // Numeric mode
    QR_MODE_AN,          // Alphabet-numeric mode
    QR_MODE_8,           // 8-bit data mode
    QR_MODE_KANJI,       // Kanji (shift-jis) mode
    QR_MODE_STRUCTURE,   // Internal use only
    QR_MODE_ECI,         // ECI mode
    QR_MODE_FNC1FIRST,   // FNC1, first position
    QR_MODE_FNC1SECOND,  // FNC1, second position
} QRencodeMode;
```

**Notes:**
- `QR_MODE_NUL` and `QR_MODE_STRUCTURE` are for internal use only
- When using auto-parsing functions (`QRcode_encodeString`), only `QR_MODE_8` and `QR_MODE_KANJI` are valid as hints
- `QR_MODE_ECI` data must be appended via `QRinput_appendECIheader()`, not `QRinput_append()`

### QRecLevel

Error correction level enumeration:

```c
typedef enum {
    QR_ECLEVEL_L = 0,  // lowest (~7% recovery)
    QR_ECLEVEL_M,      // (~15% recovery)
    QR_ECLEVEL_Q,      // (~25% recovery)
    QR_ECLEVEL_H       // highest (~30% recovery)
} QRecLevel;
```

### QRcode

The encoded QR Code symbol:

```c
typedef struct {
    int version;          // version of the symbol (1-40 for QR, 1-4 for MQR)
    int width;            // width of the symbol in modules
    unsigned char *data;  // symbol data (width*width bytes)
} QRcode;
```

The `data` array has `width * width` entries. Each byte is a module with this bit layout:

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

For rendering, typically only bit 0 is used: `data[y * width + x] & 1`.

### QRcode_List

Singly-linked list of QRcode for structured-append symbols:

```c
typedef struct _QRcode_List {
    QRcode *code;
    struct _QRcode_List *next;
} QRcode_List;
```

### QRinput

Opaque input data object:

```c
typedef struct _QRinput QRinput;
```

Internally (in `qrinput.h`), this is:
```c
struct _QRinput {
    int version;
    QRecLevel level;
    QRinput_List *head;
    QRinput_List *tail;
    int mqr;
    int fnc1;
    unsigned char appid;
};
```

### QRinput_Struct

Opaque structured input set:

```c
typedef struct _QRinput_Struct QRinput_Struct;
```

Internally:
```c
struct _QRinput_Struct {
    int size;
    int parity;
    QRinput_InputList *head;
    QRinput_InputList *tail;
};
```

---

## Constants

```c
#define QRSPEC_VERSION_MAX 40     // Maximum QR Code version
#define MQRSPEC_VERSION_MAX 4     // Maximum Micro QR version
```

---

## Input Construction Functions

### QRinput_new

```c
QRinput *QRinput_new(void);
```

Creates a new input data object with version set to 0 (auto-select) and error correction level set to `QR_ECLEVEL_L`.

**Returns:** Input object, or `NULL` on error (sets `errno` to `ENOMEM`).

**Example:**
```c
QRinput *input = QRinput_new();
if(input == NULL) {
    perror("QRinput_new");
    exit(1);
}
// Append data, encode, then free
QRinput_free(input);
```

### QRinput_new2

```c
QRinput *QRinput_new2(int version, QRecLevel level);
```

Creates a new input data object with explicit version and error correction level.

**Parameters:**
- `version` — Version number (0 for auto-select, 1–40)
- `level` — Error correction level (`QR_ECLEVEL_L` through `QR_ECLEVEL_H`)

**Returns:** Input object, or `NULL` on error.

**Errors:**
- `ENOMEM` — Unable to allocate memory
- `EINVAL` — Invalid version (< 0 or > 40) or level

**Example:**
```c
QRinput *input = QRinput_new2(5, QR_ECLEVEL_M);
```

### QRinput_newMQR

```c
QRinput *QRinput_newMQR(int version, QRecLevel level);
```

Creates a Micro QR Code input object. **Version must be specified** (> 0), unlike standard QR where 0 means auto.

**Parameters:**
- `version` — Version number (1–4)
- `level` — Error correction level. Valid combinations:
  - M1: error detection only (level is ignored)
  - M2: `QR_ECLEVEL_L`, `QR_ECLEVEL_M`
  - M3: `QR_ECLEVEL_L`, `QR_ECLEVEL_M`
  - M4: `QR_ECLEVEL_L`, `QR_ECLEVEL_M`, `QR_ECLEVEL_Q`

**Returns:** Input object (with `mqr` flag set), or `NULL`.

**Errors:**
- `EINVAL` — Invalid version/level combination (checks `MQRspec_getECCLength()` != 0)
- `ENOMEM` — Unable to allocate memory

**Example:**
```c
QRinput *input = QRinput_newMQR(3, QR_ECLEVEL_L);
```

### QRinput_append

```c
int QRinput_append(QRinput *input, QRencodeMode mode, int size,
                   const unsigned char *data);
```

Appends a data chunk to the input object. The data is **copied** — the caller retains ownership of the `data` buffer.

**Parameters:**
- `input` — Input object
- `mode` — Encoding mode (`QR_MODE_NUM`, `QR_MODE_AN`, `QR_MODE_8`, `QR_MODE_KANJI`, `QR_MODE_STRUCTURE`, `QR_MODE_ECI`, `QR_MODE_FNC1FIRST`, `QR_MODE_FNC1SECOND`)
- `size` — Size of data in bytes
- `data` — Pointer to input data

**Returns:** 0 on success, -1 on error.

**Errors:**
- `EINVAL` — Invalid data for the specified mode (e.g., non-digit data with `QR_MODE_NUM`)
- `ENOMEM` — Unable to allocate memory

**Validation rules per mode:**
- `QR_MODE_NUM`: All bytes must be '0'–'9'
- `QR_MODE_AN`: All bytes must pass `QRinput_lookAnTable()` (digits, uppercase letters, space, $, %, *, +, -, ., /, :)
- `QR_MODE_KANJI`: Even number of bytes, each pair must be valid Shift-JIS range (0x8140–0x9FFC or 0xE040–0xEBBF)
- `QR_MODE_8`: No validation (any bytes accepted)
- `QR_MODE_FNC1SECOND`: Size must be exactly 1

**Example:**
```c
QRinput *input = QRinput_new2(0, QR_ECLEVEL_M);

// Append numeric data
const unsigned char num[] = "12345";
QRinput_append(input, QR_MODE_NUM, 5, num);

// Append 8-bit data
const unsigned char text[] = "Hello, World!";
QRinput_append(input, QR_MODE_8, 13, text);

QRcode *code = QRcode_encodeInput(input);
QRinput_free(input);
```

### QRinput_appendECIheader

```c
int QRinput_appendECIheader(QRinput *input, unsigned int ecinum);
```

Appends an Extended Channel Interpretation (ECI) header.

**Parameters:**
- `input` — Input object
- `ecinum` — ECI indicator number (0–999999)

**Returns:** 0 on success, -1 on error.

**Errors:**
- `EINVAL` — `ecinum` > 999999

**Note:** Internally creates a 4-byte little-endian representation of `ecinum` and appends as `QR_MODE_ECI`.

**Example:**
```c
// Set ECI to UTF-8 (ECI 000026)
QRinput_appendECIheader(input, 26);
QRinput_append(input, QR_MODE_8, strlen(utf8_str), (unsigned char *)utf8_str);
```

### QRinput_getVersion

```c
int QRinput_getVersion(QRinput *input);
```

Returns the current version number of the input object.

**Returns:** Version number (0 for auto-select, 1–40 for QR, 1–4 for MQR).

### QRinput_setVersion

```c
int QRinput_setVersion(QRinput *input, int version);
```

Sets the version number. **Cannot be used with Micro QR objects.**

**Parameters:**
- `version` — 0 for auto-select, 1–40 for explicit version

**Returns:** 0 on success, -1 on error (`EINVAL`).

### QRinput_getErrorCorrectionLevel

```c
QRecLevel QRinput_getErrorCorrectionLevel(QRinput *input);
```

Returns the current error correction level.

### QRinput_setErrorCorrectionLevel

```c
int QRinput_setErrorCorrectionLevel(QRinput *input, QRecLevel level);
```

Sets the error correction level. **Cannot be used with Micro QR objects.**

**Returns:** 0 on success, -1 on error (`EINVAL`).

### QRinput_setVersionAndErrorCorrectionLevel

```c
int QRinput_setVersionAndErrorCorrectionLevel(QRinput *input, int version,
                                               QRecLevel level);
```

Sets both version and error correction level at once. **Recommended for Micro QR**, as it validates the combination.

**Returns:** 0 on success, -1 on error (`EINVAL`).

### QRinput_free

```c
void QRinput_free(QRinput *input);
```

Frees the input object and all data chunks contained within it. Safe to call with `NULL`.

### QRinput_check

```c
int QRinput_check(QRencodeMode mode, int size, const unsigned char *data);
```

Validates input data for the given mode without modifying any objects.

**Returns:** 0 if valid, -1 if invalid.

### QRinput_setFNC1First

```c
int QRinput_setFNC1First(QRinput *input);
```

Sets the FNC1 first position flag for GS1 compatibility.

### QRinput_setFNC1Second

```c
int QRinput_setFNC1Second(QRinput *input, unsigned char appid);
```

Sets the FNC1 second position flag with the given application identifier.

---

## Structured Append Functions

### QRinput_Struct_new

```c
QRinput_Struct *QRinput_Struct_new(void);
```

Creates a new structured input set for generating linked QR Code symbols.

**Returns:** Instance of `QRinput_Struct`, or `NULL` (`ENOMEM`).

### QRinput_Struct_setParity

```c
void QRinput_Struct_setParity(QRinput_Struct *s, unsigned char parity);
```

Manually sets the parity byte for the structured symbol set. If not called, parity is calculated automatically by `QRinput_Struct_insertStructuredAppendHeaders()`.

### QRinput_Struct_appendInput

```c
int QRinput_Struct_appendInput(QRinput_Struct *s, QRinput *input);
```

Appends a `QRinput` to the structured set. Rejects Micro QR inputs.

**Warning:** Never append the same `QRinput` object twice.

**Returns:** Number of inputs in the structure (> 0), or -1 on error.

**Errors:**
- `EINVAL` — NULL input or MQR input
- `ENOMEM` — Unable to allocate memory

### QRinput_Struct_free

```c
void QRinput_Struct_free(QRinput_Struct *s);
```

Frees the structured set and **all QRinput objects** contained within.

### QRinput_splitQRinputToStruct

```c
QRinput_Struct *QRinput_splitQRinputToStruct(QRinput *input);
```

Automatically splits a single `QRinput` into multiple inputs suitable for structured append. Calculates parity, sets it, and inserts structured-append headers.

**Prerequisites:** Version and error correction level must be set on `input`.

**Returns:** A `QRinput_Struct`, or `NULL` on error.

**Errors:**
- `ERANGE` — Input data too large
- `EINVAL` — Invalid input
- `ENOMEM` — Unable to allocate memory

### QRinput_Struct_insertStructuredAppendHeaders

```c
int QRinput_Struct_insertStructuredAppendHeaders(QRinput_Struct *s);
```

Inserts structured-append headers into each QRinput in the set. Calculates parity if not already set. **Call this exactly once before encoding.**

**Returns:** 0 on success, -1 on error.

---

## Encoding Functions — Standard QR

### QRcode_encodeInput

```c
QRcode *QRcode_encodeInput(QRinput *input);
```

Encodes a manually constructed `QRinput` into a QR Code. Dispatches to `QRcode_encodeMask()` or `QRcode_encodeMaskMQR()` based on the input's `mqr` flag.

**Warning:** THREAD UNSAFE when pthread is disabled.

**Returns:** `QRcode` instance. The result version may be larger than specified.

**Errors:**
- `EINVAL` — Invalid input object
- `ENOMEM` — Unable to allocate memory

**Example:**
```c
QRinput *input = QRinput_new2(0, QR_ECLEVEL_H);
QRinput_append(input, QR_MODE_8, 11, (unsigned char *)"Hello World");
QRcode *qr = QRcode_encodeInput(input);
// Use qr->data, qr->width, qr->version
QRcode_free(qr);
QRinput_free(input);
```

### QRcode_encodeString

```c
QRcode *QRcode_encodeString(const char *string, int version, QRecLevel level,
                             QRencodeMode hint, int casesensitive);
```

The primary high-level encoding function. Automatically parses the input string and selects optimal encoding modes.

**Parameters:**
- `string` — NUL-terminated input string
- `version` — Minimum version (0 for auto)
- `level` — Error correction level
- `hint` — Only `QR_MODE_8` or `QR_MODE_KANJI` are valid:
  - `QR_MODE_KANJI`: Assumes Shift-JIS input, encodes Kanji characters in Kanji mode
  - `QR_MODE_8`: All non-alphanumeric characters encoded as 8-bit (use for UTF-8)
- `casesensitive` — 1 for case-sensitive, 0 to convert lowercase to uppercase (enables more AN-mode encoding)

**Returns:** `QRcode` instance or `NULL`.

**Errors:**
- `EINVAL` — NULL string or invalid hint (not `QR_MODE_8` or `QR_MODE_KANJI`)
- `ENOMEM` — Unable to allocate memory
- `ERANGE` — Input too large for any version

**Implementation:** Internally calls `Split_splitStringToQRinput()` to optimize mode selection, then `QRcode_encodeInput()`.

**Example:**
```c
// Simple encoding
QRcode *qr = QRcode_encodeString("https://example.com", 0,
                                  QR_ECLEVEL_M, QR_MODE_8, 1);

// Case-insensitive (more compact)
QRcode *qr = QRcode_encodeString("HELLO123", 0,
                                  QR_ECLEVEL_L, QR_MODE_8, 0);

// With Kanji support
QRcode *qr = QRcode_encodeString(sjis_string, 0,
                                  QR_ECLEVEL_M, QR_MODE_KANJI, 1);
```

### QRcode_encodeString8bit

```c
QRcode *QRcode_encodeString8bit(const char *string, int version, QRecLevel level);
```

Encodes the entire string in 8-bit mode without mode optimization.

**Parameters:**
- `string` — NUL-terminated input string
- `version` — Minimum version (0 for auto)
- `level` — Error correction level

**Returns:** `QRcode` instance or `NULL`.

**Note:** Internally calls `QRcode_encodeData()` with `strlen(string)`.

### QRcode_encodeData

```c
QRcode *QRcode_encodeData(int size, const unsigned char *data, int version,
                           QRecLevel level);
```

Encodes raw byte data (may include NUL bytes) in 8-bit mode.

**Parameters:**
- `size` — Size of input data in bytes
- `data` — Pointer to input data
- `version` — Minimum version (0 for auto)
- `level` — Error correction level

**Returns:** `QRcode` instance or `NULL`.

**Example:**
```c
// Encoding binary data (may contain NUL)
unsigned char binary[] = {0x00, 0xFF, 0x42, 0x00, 0x7A};
QRcode *qr = QRcode_encodeData(5, binary, 0, QR_ECLEVEL_L);
```

### QRcode_free

```c
void QRcode_free(QRcode *qrcode);
```

Frees a `QRcode` instance, including the internal `data` array. Safe to call with `NULL`.

---

## Encoding Functions — Micro QR

### QRcode_encodeStringMQR

```c
QRcode *QRcode_encodeStringMQR(const char *string, int version, QRecLevel level,
                                QRencodeMode hint, int casesensitive);
```

Micro QR version of `QRcode_encodeString()`. If `version` is 0, it tries versions 1 through 4 until encoding succeeds.

**Implementation detail:** Loops from `version` to `MQRSPEC_VERSION_MAX`:
```c
for(i = version; i <= MQRSPEC_VERSION_MAX; i++) {
    QRcode *code = QRcode_encodeStringReal(string, i, level, 1, hint, casesensitive);
    if(code != NULL) return code;
}
```

### QRcode_encodeString8bitMQR

```c
QRcode *QRcode_encodeString8bitMQR(const char *string, int version, QRecLevel level);
```

8-bit encoding for Micro QR. Tries versions incrementally.

### QRcode_encodeDataMQR

```c
QRcode *QRcode_encodeDataMQR(int size, const unsigned char *data, int version,
                              QRecLevel level);
```

Raw data encoding for Micro QR. Tries versions incrementally.

---

## Encoding Functions — Structured Append

### QRcode_encodeInputStructured

```c
QRcode_List *QRcode_encodeInputStructured(QRinput_Struct *s);
```

Encodes each `QRinput` in a structured set into a linked list of `QRcode` objects.

**Returns:** Head of `QRcode_List`, or `NULL` on error.

### QRcode_encodeStringStructured

```c
QRcode_List *QRcode_encodeStringStructured(const char *string, int version,
                                            QRecLevel level, QRencodeMode hint,
                                            int casesensitive);
```

Auto-splits a string and encodes as structured-append symbols.

**Parameters:** Same as `QRcode_encodeString()`, but `version` must be specified (non-zero).

**Returns:** Head of `QRcode_List`.

**Example:**
```c
QRcode_List *list = QRcode_encodeStringStructured(long_text, 5,
                                                   QR_ECLEVEL_M, QR_MODE_8, 1);
QRcode_List *entry = list;
int i = 1;
while(entry != NULL) {
    QRcode *qr = entry->code;
    printf("Symbol %d: version=%d, width=%d\n", i, qr->version, qr->width);
    // render qr
    entry = entry->next;
    i++;
}
QRcode_List_free(list);
```

### QRcode_encodeString8bitStructured

```c
QRcode_List *QRcode_encodeString8bitStructured(const char *string, int version,
                                                QRecLevel level);
```

8-bit structured encoding.

### QRcode_encodeDataStructured

```c
QRcode_List *QRcode_encodeDataStructured(int size, const unsigned char *data,
                                          int version, QRecLevel level);
```

Raw data structured encoding.

### QRcode_List_size

```c
int QRcode_List_size(QRcode_List *qrlist);
```

Returns the number of QRcode objects in the linked list.

### QRcode_List_free

```c
void QRcode_List_free(QRcode_List *qrlist);
```

Frees the entire linked list and all contained `QRcode` objects.

---

## Utility Functions

### QRcode_APIVersion

```c
void QRcode_APIVersion(int *major_version, int *minor_version, int *micro_version);
```

Retrieves the library version as three integers.

**Example:**
```c
int major, minor, micro;
QRcode_APIVersion(&major, &minor, &micro);
printf("libqrencode %d.%d.%d\n", major, minor, micro);
// Output: libqrencode 4.1.1
```

### QRcode_APIVersionString

```c
char *QRcode_APIVersionString(void);
```

Returns a string identifying the library version. The string is held by the library — do **NOT** free it.

**Returns:** Version string (e.g., `"4.1.1"`).

### QRcode_clearCache

```c
void QRcode_clearCache(void);  // DEPRECATED
```

A deprecated no-op function. Previously cleared internal frame caches. Marked with `__attribute__((deprecated))` on GCC.

---

## Complete Usage Examples

### Minimal Example

```c
#include <stdio.h>
#include <qrencode.h>

int main(void) {
    QRcode *qr = QRcode_encodeString("Hello", 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if(qr == NULL) {
        perror("encoding failed");
        return 1;
    }

    printf("Version: %d, Width: %d\n", qr->version, qr->width);

    for(int y = 0; y < qr->width; y++) {
        for(int x = 0; x < qr->width; x++) {
            printf("%s", (qr->data[y * qr->width + x] & 1) ? "##" : "  ");
        }
        printf("\n");
    }

    QRcode_free(qr);
    return 0;
}
```

### Manual Input Construction

```c
#include <qrencode.h>

int main(void) {
    QRinput *input = QRinput_new2(0, QR_ECLEVEL_M);

    // Mixed-mode encoding: numbers in numeric mode, text in 8-bit
    QRinput_append(input, QR_MODE_NUM, 10, (unsigned char *)"0123456789");
    QRinput_append(input, QR_MODE_8, 5, (unsigned char *)"Hello");
    QRinput_append(input, QR_MODE_NUM, 3, (unsigned char *)"999");

    QRcode *qr = QRcode_encodeInput(input);
    // ... use qr ...

    QRcode_free(qr);
    QRinput_free(input);
    return 0;
}
```

### ECI Header for UTF-8

```c
#include <qrencode.h>

int main(void) {
    QRinput *input = QRinput_new2(0, QR_ECLEVEL_M);

    // Add ECI header for UTF-8 (ECI 000026)
    QRinput_appendECIheader(input, 26);

    // Append UTF-8 data
    const char *utf8 = "こんにちは";  // UTF-8 encoded
    QRinput_append(input, QR_MODE_8, strlen(utf8), (unsigned char *)utf8);

    QRcode *qr = QRcode_encodeInput(input);
    // ... use qr ...

    QRcode_free(qr);
    QRinput_free(input);
    return 0;
}
```

### Structured Append

```c
#include <qrencode.h>

int main(void) {
    // Automatically split and encode
    const char *long_data = "...very long text...";
    QRcode_List *list = QRcode_encodeStringStructured(
        long_data, 5, QR_ECLEVEL_M, QR_MODE_8, 1);

    if(list == NULL) {
        perror("structured encoding failed");
        return 1;
    }

    printf("Total symbols: %d\n", QRcode_List_size(list));

    QRcode_List *entry = list;
    while(entry != NULL) {
        // Render entry->code
        entry = entry->next;
    }

    QRcode_List_free(list);
    return 0;
}
```

### Micro QR

```c
#include <qrencode.h>

int main(void) {
    // Encode short data in Micro QR
    QRcode *qr = QRcode_encodeStringMQR("12345", 0, QR_ECLEVEL_L,
                                         QR_MODE_8, 1);
    if(qr != NULL) {
        printf("MQR version: M%d, width: %d\n", qr->version, qr->width);
        QRcode_free(qr);
    }
    return 0;
}
```

### Binary Data with NUL Bytes

```c
#include <qrencode.h>

int main(void) {
    // QRcode_encodeString cannot handle NUL bytes — use QRcode_encodeData
    unsigned char binary_data[] = {0x48, 0x65, 0x00, 0x6C, 0x6F};
    QRcode *qr = QRcode_encodeData(5, binary_data, 0, QR_ECLEVEL_L);

    if(qr != NULL) {
        // ... use qr ...
        QRcode_free(qr);
    }
    return 0;
}
```

### FNC1 (GS1) Mode

```c
#include <qrencode.h>

int main(void) {
    QRinput *input = QRinput_new2(0, QR_ECLEVEL_M);

    // Set GS1 mode
    QRinput_setFNC1First(input);

    // Append GS1 data
    const char *gs1 = "(01)12345678901234";
    QRinput_append(input, QR_MODE_8, strlen(gs1), (unsigned char *)gs1);

    QRcode *qr = QRcode_encodeInput(input);
    // ...
    QRcode_free(qr);
    QRinput_free(input);
    return 0;
}
```

---

## Thread Safety Notes

All `QRcode_encode*` functions are documented as:

> **Warning:** This function is THREAD UNSAFE when pthread is disabled.

When built with pthread support (`HAVE_LIBPTHREAD`):
- The Reed-Solomon initialization is protected by `RSECC_mutex`
- Multiple threads can safely call encoding functions concurrently

When built without pthread support:
- The lazy initialization of GF(2^8) tables and generator polynomials is not thread-safe
- Concurrent calls may corrupt internal state
- Use external synchronization or call `QRcode_encodeString()` once from the main thread before spawning worker threads (to trigger initialization)

---

## Error Convention Summary

| Return Type | Success | Failure |
|---|---|---|
| `QRcode *` | Valid pointer | `NULL` (check `errno`) |
| `QRcode_List *` | Valid pointer | `NULL` (check `errno`) |
| `QRinput *` | Valid pointer | `NULL` (check `errno`) |
| `QRinput_Struct *` | Valid pointer | `NULL` (check `errno`) |
| `int` (status) | `0` | `-1` (check `errno`) |
| `int` (count) | `> 0` | `-1` (check `errno`) |
| `char *` (version string) | Non-NULL | N/A (always succeeds) |
