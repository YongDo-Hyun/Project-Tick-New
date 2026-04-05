# genqrcode / libqrencode — Testing

## Test Infrastructure

### Test Framework

libqrencode uses a custom test framework defined in `tests/common.h`:

```c
#define testStart(__arg__) { \
    int tests = 0, failed = 0, assertion; \
    char *_test_name = (__arg__); \
    fputs("--- TEST: ", stderr); \
    fputs(_test_name, stderr); \
    fputs(" ---\n", stderr);

#define testEnd() \
    if(failed) fputs("FAILED.\n", stderr); \
    else fputs("PASSED.\n", stderr); \
    testFinish(); }

#define testFinish() \
    printf(" %s: %d assertions, %d failures.\n", \
           _test_name, tests, failed);

#define assert_equal(__actual__, __expected__, ...) { \
    tests++; assertion = (__actual__ == __expected__); \
    if(!assertion) { \
        failed++; \
        printf("FAILED: " __VA_ARGS__); \
        printf(" (%d != %d)\n", __actual__, __expected__); \
    } }

#define assert_zero(__expr__, ...) \
    assert_equal((__expr__), 0, __VA_ARGS__)

#define assert_nonzero(__expr__, ...) { \
    tests++; assertion = (__expr__) != 0; \
    if(!assertion) { \
        failed++; \
        printf("FAILED: " __VA_ARGS__); \
        printf("\n"); \
    } }

#define assert_null(__expr__, ...) { \
    tests++; assertion = (__expr__) == NULL; \
    if(!assertion) { \
        failed++; \
        printf("FAILED: " __VA_ARGS__); \
        printf("\n"); \
    } }

#define assert_nonnull(__expr__, ...) { \
    tests++; assertion = (__expr__) != NULL; \
    if(!assertion) { \
        failed++; \
        printf("FAILED: " __VA_ARGS__); \
        printf("\n"); \
    } }

#define assert_nothing(__expr__, ...) { \
    tests++; __expr__; }
```

Test functions return void and use `testStart()`/`testEnd()` blocks. Assertions increment the `tests` counter and only print output on failure.

---

## Test Programs

### test_bitstream

Tests the `BitStream` module:

- `test_null()` — New stream has length 0
- `test_num()` — `BitStream_appendNum()` with various bit widths
- `test_bytes()` — `BitStream_appendBytes()` correctness
- `test_toByte()` — Bit packing from 1-bit-per-byte to 8-bits-per-byte

### test_estimatebit

Tests bit estimation functions:

- `test_estimateBitsModeNum()` — Numeric mode bit cost for various lengths
- `test_estimateBitsModeAn()` — Alphanumeric mode bit cost
- `test_estimateBitsMode8()` — 8-bit mode bit cost
- `test_estimateBitsModeKanji()` — Kanji mode bit cost

### test_qrinput

Tests input data management:

- `test_encodeNumeric()` — Numeric data encoding correctness
- `test_encodeAn()` — Alphanumeric encoding with the AN table
- `test_encode8()` — 8-bit byte encoding
- `test_encodeKanji()` — Kanji double-byte encoding
- `test_encodeTooLong()` — Overflow detection (ERANGE)
- `test_struct()` — Structured append input handling
- `test_splitEntry()` — Input entry splitting for version changes
- `test_parity()` — Parity calculation for structured append
- `test_padding()` — Pad byte alternation (0xEC/0x11)
- `test_mqr()` — Micro QR input creation and validation

### test_qrspec

Tests QR Code specification tables:

- `test_getWidth()` — Width = version × 4 + 17
- `test_getDataLength()` — Data capacity lookup
- `test_getECCLength()` — ECC length calculation
- `test_alignment()` — Alignment pattern positions
- `test_versionPattern()` — Version info BCH codes
- `test_formatInfo()` — Format info BCH codes

### test_mqrspec

Tests Micro QR spec tables:

- `test_getWidth()` — Width = version × 2 + 9
- `test_getDataLengthBit()` — Bit-level data capacity
- `test_getECCLength()` — MQR ECC lengths
- `test_newFrame()` — Frame creation with single finder pattern

### test_rs

Tests Reed-Solomon encoder:

- Tests `RSECC_encode()` with known data/ECC pairs

### test_qrencode

Tests the high-level encoding pipeline:

- `test_encode()` — Full encode and verify symbol dimensions
- `test_encodeVersion()` — Version auto-selection
- `test_encodeMQR()` — Micro QR encoding
- `test_encode8bitString()` — 8-bit string encoding
- `test_encodeStructured()` — Structured append output
- `test_encodeMask()` — Forced mask testing
- `test_encodeEmpty()` — Empty input handling

### test_split

Tests the string splitter:

- `test_split1()` — Pure numeric string
- `test_split2()` — Pure alphanumeric
- `test_split3()` — Mixed numeric/alpha/8-bit
- `test_split4()` — Kanji detection
- `test_split5()` — Mode boundary optimization

### test_split_urls

Tests URL splitting efficiency:

- Tests that common URL patterns produce efficient mode sequences
- Verifies that `http://`, domain names, and paths are encoded optimally

### test_mask

Tests full QR masking:

- Tests all 8 mask patterns (`Mask_mask0` through `Mask_mask7`)
- `test_eval()` — Penalty evaluation
- `test_format()` — Format information writing
- `test_maskSelection()` — Automatic mask selection picks minimum penalty

### test_mmask

Tests Micro QR masking:

- Tests 4 mask patterns (`MMask_mask0` through `MMask_mask3`)
- `test_eval()` — Micro QR scoring (maximize, not minimize)
- `test_format()` — MQR format information

### test_monkey

Random/fuzz testing:

- Generates random input data of various sizes and modes
- Encodes and verifies the result is non-NULL and has valid dimensions

---

## Utility Programs

### prof_qrencode

Profiling harness. Encodes the same string many times for performance measurement:

```c
int main() {
    // Repeated encoding for profiling with gprof/gcov
}
```

Build with `GPROF` or `COVERAGE` CMake options.

### pthread_qrencode

Thread safety test. Spawns multiple threads encoding simultaneously:

```c
// Tests concurrent RSECC_encode() calls
// Verifies no crashes or data corruption
```

Only built when `HAVE_LIBPTHREAD` is defined.

### view_qrcode

Renders a QR code to the terminal for visual inspection:

```c
// ASCII art output of encoded symbol
```

### create_frame_pattern

Generates and prints the function pattern frame (finder, timing, alignment):

```c
// Used to visually verify frame creation
```

---

## Running Tests

### Via CMake

```bash
mkdir build && cd build
cmake -DWITH_TESTS=ON ..
make
make test
# or:
ctest --verbose
```

CMake `tests/CMakeLists.txt` registers test executables:

```cmake
add_executable(test_qrinput test_qrinput.c common.c)
target_link_libraries(test_qrinput qrencode_static)
add_test(test_qrinput test_qrinput)

add_executable(test_bitstream test_bitstream.c common.c)
target_link_libraries(test_bitstream qrencode_static)
add_test(test_bitstream test_bitstream)

# ... similar for all test programs
```

### Via Autotools

```bash
./configure --with-tests
make
make check
```

Or use the test scripts directly:

```bash
cd tests
./test_basic.sh
```

### test_basic.sh

```bash
#!/bin/sh
./test_bitstream && \
./test_estimatebit && \
./test_qrinput && \
./test_qrspec && \
./test_mqrspec && \
./test_rs && \
./test_qrencode && \
./test_split && \
./test_split_urls && \
./test_mask && \
./test_mmask && \
./test_monkey
```

Runs all test executables sequentially. Exits on first failure due to `&&` chaining.

### test_all.sh

Runs `test_basic.sh` plus `test_configure.sh` (which tests the autotools configure/build cycle).

---

## Test Build Macros

When tests are enabled:

- `WITH_TESTS` is defined → `STATIC_IN_RELEASE` is NOT defined
- All `STATIC_IN_RELEASE` functions become externally visible
- `qrencode_inner.h` exposes internal types (`RSblock`, `QRRawCode`, etc.)

This allows tests to call internal functions directly:

```c
// In test_mask.c:
#include "../qrencode_inner.h"

void test_mask() {
    testStart("Testing mask evaluation");
    QRcode *code = QRcode_encodeMask(input, 0);  // force mask 0
    // ... verify mask application ...
    testEnd();
}
```

---

## Code Coverage

### With gcov (Autotools)

```bash
./configure --enable-gcov --with-tests
make
make check
gcov *.c
```

### With CMake

```bash
cmake -DCOVERAGE=ON -DWITH_TESTS=ON ..
make
make test
# generate coverage report
```

CMake adds `-fprofile-arcs -ftest-coverage` flags when `COVERAGE` is ON.

---

## Address Sanitizer

```bash
# CMake:
cmake -DASAN=ON -DWITH_TESTS=ON ..

# Autotools:
./configure --enable-asan --with-tests
```

Adds `-fsanitize=address -fno-omit-frame-pointer` for memory error detection.

---

## Adding a New Test

1. Create `tests/test_newmodule.c`:
```c
#include <stdio.h>
#include "common.h"
#include "../module.h"

void test_something(void)
{
    testStart("Testing something");
    int result = module_function(42);
    assert_equal(result, 84, "module_function(42)");
    testEnd();
}

int main(void)
{
    test_something();
    return 0;
}
```

2. Add to `tests/CMakeLists.txt`:
```cmake
add_executable(test_newmodule test_newmodule.c common.c)
target_link_libraries(test_newmodule qrencode_static)
add_test(test_newmodule test_newmodule)
```

3. Add to `tests/Makefile.am`:
```makefile
noinst_PROGRAMS += test_newmodule
test_newmodule_SOURCES = test_newmodule.c common.h common.c
test_newmodule_LDADD = ../libqrencode.la
```

4. Add to `tests/test_basic.sh`:
```bash
./test_newmodule && \
```
