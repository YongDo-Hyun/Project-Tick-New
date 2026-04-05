# genqrcode / libqrencode — Code Style and Conventions

## Naming Conventions

### Module Prefixes

Every function and type is prefixed with its module:

| Prefix | Module | File |
|---|---|---|
| `QRcode_` | Encoded symbol / high-level API | `qrencode.c` |
| `QRinput_` | Input data management | `qrinput.c` |
| `QRinput_Struct_` | Structured append input | `qrinput.c` |
| `QRspec_` | QR Code spec tables | `qrspec.c` |
| `MQRspec_` | Micro QR spec tables | `mqrspec.c` |
| `QRraw_` | Raw RS block management | `qrencode.c` |
| `MQRraw_` | Micro QR raw management | `qrencode.c` |
| `Mask_` | Full QR masking | `mask.c` |
| `MMask_` | Micro QR masking | `mmask.c` |
| `RSECC_` | Reed-Solomon encoder | `rsecc.c` |
| `BitStream_` | Bit stream builder | `bitstream.c` |
| `Split_` | Input string splitter | `split.c` |
| `FrameFiller_` | Module placement | `qrencode.c` |

### Function Naming Patterns

- `*_new()` / `*_free()` — Constructor / destructor pairs
- `*_init()` — In-place initialization (no allocation)
- `*_get*()` / `*_set*()` — Accessor / mutator pairs
- `*_encode*()` — Encoding operations
- `*_check*()` — Validation without side effects
- `*_estimate*()` — Capacity estimation

### Static Functions

Internal functions use the module prefix plus a descriptive name:

```c
static int QRinput_encodeModeNum(QRinput_List *entry, int version, int mqr);
static int QRinput_checkModeNum(int size, const char *data);
static int QRinput_estimateBitsModeNum(int size);
```

---

## STATIC_IN_RELEASE Macro

A key pattern for testability — internal functions are static in release builds but visible in test builds:

```c
#ifdef STATIC_IN_RELEASE
#define STATIC_IN_RELEASE static
#else
#define STATIC_IN_RELEASE
#endif
```

When `WITH_TESTS` is defined (via CMake or autotools), `STATIC_IN_RELEASE` is undefined and internal functions get external linkage, making them callable from test programs.

Usage throughout the codebase:

```c
STATIC_IN_RELEASE int BitStream_appendNum(BitStream *bstream, size_t bits, unsigned int num);
STATIC_IN_RELEASE int BitStream_appendBytes(BitStream *bstream, size_t size, unsigned char *data);
```

This avoids `__attribute__((visibility("default")))` hacks — simply controlling `static` linkage via a macro.

---

## The `qrencode_inner.h` Header

Test programs include `qrencode_inner.h` instead of (or in addition to) `qrencode.h`. This header exposes:

```c
// Internal struct types
typedef struct { ... } RSblock;
typedef struct { ... } QRRawCode;
typedef struct { ... } MQRRawCode;

// Internal functions for testing
extern unsigned char *FrameFiller_test(int version);
extern QRcode *QRcode_encodeMask(QRinput *input, int mask);
extern QRcode *QRcode_encodeMaskMQR(QRinput *input, int mask);
extern QRcode *QRcode_new(int version, int width, unsigned char *data);
```

This separation keeps the public API clean while enabling thorough unit testing.

---

## Error Handling Pattern

### errno-Based Errors

All functions that can fail set `errno` and return a sentinel:

```c
// Pointer-returning functions: return NULL on error
QRinput *QRinput_new2(int version, QRecLevel level)
{
    if(version < 0 || version > QRSPEC_VERSION_MAX) {
        errno = EINVAL;
        return NULL;
    }
    if(/* invalid level */) {
        errno = EINVAL;
        return NULL;
    }
    // ...
    input = malloc(sizeof(QRinput));
    if(input == NULL) {
        // errno set by malloc (ENOMEM)
        return NULL;
    }
    // ...
}

// int-returning functions: return -1 on error
int QRinput_append(QRinput *input, QRencodeMode mode, int size,
                   const unsigned char *data)
{
    int ret = QRinput_check(mode, size, data);
    if(ret != 0) {
        errno = EINVAL;
        return -1;
    }
    // ...
}
```

### Common errno Values

- `EINVAL` — Invalid parameters
- `ENOMEM` — Memory allocation failure
- `ERANGE` — Data too large for any QR version

---

## Memory Management

### Ownership Model

- `QRinput_append()` **copies** its data — the caller retains ownership
- `QRcode_encodeInput()` does **not** free the input — caller must free both
- `QRinput_Struct_free()` frees all contained `QRinput` objects
- `QRcode_List_free()` frees all contained `QRcode` objects
- `QRcode_free()` frees the `data` array inside the `QRcode`

### Allocation Patterns

```c
// Typical constructor pattern
QRinput *QRinput_new(void)
{
    QRinput *input;
    input = (QRinput *)malloc(sizeof(QRinput));
    if(input == NULL) return NULL;
    // initialize fields
    input->head = NULL;
    input->tail = NULL;
    // ...
    return input;
}

// Typical destructor pattern — walk linked list
void QRinput_free(QRinput *input)
{
    if(input != NULL) {
        QRinput_List *list = input->head;
        while(list != NULL) {
            QRinput_List *next = list->next;
            // free list entry data
            if(list->data) free(list->data);
            if(list->bstream) BitStream_free(list->bstream);
            free(list);
            list = next;
        }
        free(input);
    }
}
```

### BitStream Growth

`BitStream` uses doubling strategy:

```c
#define DEFAULT_BUFSIZE 128

static int BitStream_allocate(BitStream *bstream, size_t length)
{
    // doubles capacity until sufficient
    while(bstream->datasize < length) {
        bstream->datasize *= 2;
    }
    bstream->data = realloc(bstream->data, bstream->datasize);
}
```

---

## Macro Usage

### Spec Accessor Macros

Five macros for RS block spec access (from `qrspec.h`):

```c
#define QRspec_rsBlockNum(__spec__)    (__spec__[0] + __spec__[3])
#define QRspec_rsBlockNum1(__spec__)   (__spec__[0])
#define QRspec_rsDataCodes1(__spec__)  (__spec__[1])
#define QRspec_rsEccCodes1(__spec__)   (__spec__[2])
#define QRspec_rsBlockNum2(__spec__)   (__spec__[3])
#define QRspec_rsDataCodes2(__spec__)  (__spec__[4])
#define QRspec_rsEccCodes2(__spec__)   (__spec__[2])
```

### MASKMAKER

Code generation macro for mask functions (see masking docs):

```c
#define MASKMAKER(__exp__) \
    int x, y;\
    int b = 0;\
    for(y = 0; y < width; y++) {\
        for(x = 0; x < width; x++) {\
            if(*s & 0x80) { *d = *s; }\
            else { *d = *s ^ ((__exp__) == 0); }\
            s++; d++;\
        }\
    }\
    return b;
```

---

## Header Include Pattern

Each module follows a consistent pattern:

```c
// In .c files:
#include "config.h"         // generated by autotools/cmake
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "qrencode.h"       // public header (if needed)
#include "qrencode_inner.h" // internal declarations (if needed)
#include "module.h"         // own header
```

### Config Macros

Key `config.h` defines checked throughout:
- `HAVE_CONFIG_H` — autotools-generated config
- `HAVE_LIBPTHREAD` — pthread support available
- `STATIC_IN_RELEASE` — controls function visibility
- `WITH_TESTS` — test build mode
- `HAVE_STRDUP` — strdup availability

---

## Data Type Conventions

- `unsigned char *` for binary data and QR module arrays
- `int` for sizes, lengths, and error returns
- `unsigned int` for bit values and format info
- `signed char` for the AN lookup table (needs -1 sentinel)
- Opaque types via `typedef struct _Name Name` pattern

---

## Const Correctness

Input data parameters are consistently `const`:

```c
int RSECC_encode(int data_length, int ecc_length,
                 const unsigned char *data, unsigned char *ecc);

int QRinput_append(QRinput *input, QRencodeMode mode, int size,
                   const unsigned char *data);
```

Spec tables are `static const`:

```c
static const QRspec_Capacity qrspecCapacity[QRSPEC_VERSION_MAX + 1] = { ... };
static const int eccTable[QRSPEC_VERSION_MAX+1][4][2] = { ... };
static const unsigned int formatInfo[4][8] = { ... };
static const signed char QRinput_anTable[128] = { ... };
```

---

## Thread Safety Approach

The library uses a minimal locking strategy:

1. **Single mutex** in `rsecc.c`: `RSECC_mutex` protects lazy initialization of GF tables and generator polynomials
2. **No global mutable state** after initialization — spec tables are `const`, generator polynomials become read-only after first build
3. **Local allocations** in encoding functions — each call creates its own frame, mask buffers, etc.
4. **Conditional compilation**: `#ifdef HAVE_LIBPTHREAD` guards all pthread usage

---

## Build-System Integration

### CMake Conditionals

```cmake
if(WITH_TESTS)
    add_definitions(-DWITH_TESTS)
endif()
```

When `WITH_TESTS` is on, `STATIC_IN_RELEASE` is not defined, exposing internal functions.

### Autotools Conditionals

```m4
AC_ARG_WITH([tests],
  [AS_HELP_STRING([--with-tests], [build tests])],
  [], [with_tests=no])
if test x$with_tests = xyes ; then
    AC_DEFINE([WITH_TESTS], [1])
fi
```

---

## File Organization

Each module is a `.c` / `.h` pair:

- `bitstream.c` / `bitstream.h` — Bit stream container
- `mask.c` / `mask.h` — QR masking and penalty
- `mmask.c` / `mmask.h` — Micro QR masking
- `qrencode.c` / `qrencode.h` — Public API + encoder core
- `qrinput.c` / `qrinput.h` — Input management + mode encoding
- `qrspec.c` / `qrspec.h` — QR Code specification tables
- `mqrspec.c` / `mqrspec.h` — Micro QR specification tables
- `rsecc.c` / `rsecc.h` — Reed-Solomon encoder
- `split.c` / `split.h` — String splitter / mode optimizer

Standalone files:
- `qrencode_inner.h` — Test-only internal declarations
- `qrenc.c` — CLI tool (not a library module)
