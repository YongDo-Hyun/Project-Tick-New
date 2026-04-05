# cmark — Testing

## Overview

cmark has a multi-layered testing infrastructure: C API unit tests, spec conformance tests, pathological input tests, fuzz testing, and memory sanitizers. The build system integrates all of these through CMake and CTest.

## Test Infrastructure (CMakeLists.txt)

### API Tests

```cmake
add_executable(api_test api_test/main.c)
target_link_libraries(api_test libcmark_static)
add_test(NAME api_test COMMAND api_test)
```

The API test executable links against the static library and tests the public C API directly.

### Spec Tests

```cmake
add_test(NAME spec_test
         COMMAND ${PYTHON_EXECUTABLE} test/spec_tests.py
                 --spec test/spec.txt
                 --program $<TARGET_FILE:cmark>)
```

Spec tests run the `cmark` binary against the CommonMark specification. The Python script `spec_tests.py` parses the spec file, extracts input/output examples, runs `cmark` on each input, and compares the output.

### Pathological Tests

```cmake
add_test(NAME pathological_test
         COMMAND ${PYTHON_EXECUTABLE} test/pathological_tests.py
                 --program $<TARGET_FILE:cmark>)
```

These tests verify that cmark handles pathological inputs (deeply nested structures, long runs of special characters) without excessive time or memory usage.

### Smart Punctuation Tests

```cmake
add_test(NAME smart_punct_test
         COMMAND ${PYTHON_EXECUTABLE} test/spec_tests.py
                 --spec test/smart_punct.txt
                 --program $<TARGET_FILE:cmark>
                 --extensions "")
```

Tests for the `CMARK_OPT_SMART` option (curly quotes, em/en dashes, ellipses).

### Roundtrip Tests

```cmake
add_test(NAME roundtrip_test
         COMMAND ${PYTHON_EXECUTABLE} test/roundtrip_tests.py
                 --spec test/spec.txt
                 --program $<TARGET_FILE:cmark>)
```

Roundtrip tests verify that `cmark -t commonmark | cmark -t html` produces the same HTML as direct `cmark -t html`.

### Entity Tests

```cmake
add_test(NAME entity_test
         COMMAND ${PYTHON_EXECUTABLE} test/spec_tests.py
                 --spec test/entity.txt
                 --program $<TARGET_FILE:cmark>)
```

Tests HTML entity handling.

### Regression Tests

```cmake
add_test(NAME regression_test
         COMMAND ${PYTHON_EXECUTABLE} test/spec_tests.py
                 --spec test/regression.txt
                 --program $<TARGET_FILE:cmark>)
```

Regression tests cover previously discovered bugs.

## API Test (`api_test/main.c`)

The API test file is a single C source file with test functions covering every public API function. Test patterns used:

### Test Macros

```c
#define OK(test, msg) \
  if (test) { passes++; } \
  else { failures++; fprintf(stderr, "FAIL: %s\n  %s\n", __func__, msg); }

#define INT_EQ(actual, expected, msg) \
  if ((actual) == (expected)) { passes++; } \
  else { failures++; fprintf(stderr, "FAIL: %s\n  Expected %d got %d: %s\n", \
         __func__, expected, actual, msg); }

#define STR_EQ(actual, expected, msg) \
  if (strcmp(actual, expected) == 0) { passes++; } \
  else { failures++; fprintf(stderr, "FAIL: %s\n  Expected \"%s\" got \"%s\": %s\n", \
         __func__, expected, actual, msg); }
```

### Test Categories

1. **Version tests**: Verify `cmark_version()` and `cmark_version_string()` return correct values
2. **Constructor tests**: `cmark_node_new()` for each node type
3. **Accessor tests**: Get/set for heading level, list type, list tight, content, etc.
4. **Tree manipulation tests**: `cmark_node_append_child()`, `cmark_node_insert_before()`, etc.
5. **Parser tests**: `cmark_parse_document()`, streaming `cmark_parser_feed()` + `cmark_parser_finish()`
6. **Renderer tests**: Verify HTML, XML, man, LaTeX, CommonMark output for known inputs
7. **Iterator tests**: `cmark_iter_new()`, traversal order, `cmark_iter_reset()`
8. **Memory tests**: Custom allocator, `cmark_node_free()`, no leaks

### Example Test Function

```c
static void test_md_to_html(const char *markdown, const char *expected_html,
                            const char *msg) {
  char *html = cmark_markdown_to_html(markdown, strlen(markdown),
                                      CMARK_OPT_DEFAULT);
  STR_EQ(html, expected_html, msg);
  free(html);
}
```

## Spec Test Format

The spec file (`test/spec.txt`) uses a specific format:

```
```````````````````````````````` example
Markdown input here
.
<p>Expected HTML output here</p>
````````````````````````````````
```

Each example is delimited by `example` markers. The `.` on a line by itself separates input from expected output.

The Python test runner (`test/spec_tests.py`):
1. Parses the spec file to extract examples
2. For each example, runs the `cmark` binary with the input
3. Compares the actual output with the expected output
4. Reports pass/fail for each example

## Pathological Input Tests

The pathological test file (`test/pathological_tests.py`) generates adversarial inputs designed to trigger worst-case behavior:

- Deeply nested block quotes (`> > > > > ...`)
- Deeply nested lists
- Long runs of backticks
- Many consecutive closing brackets `]]]]]...]`
- Long emphasis delimiter runs `***...***`
- Repeated link definitions

Each test verifies that cmark completes within a reasonable time bound (not quadratic or exponential).

## Fuzzing

### LibFuzzer

```cmake
if(CMARK_LIB_FUZZER)
  add_executable(cmark_fuzz fuzz/cmark_fuzz.c)
  target_link_libraries(cmark_fuzz libcmark_static)
  target_compile_options(cmark_fuzz PRIVATE -fsanitize=fuzzer)
  target_link_options(cmark_fuzz PRIVATE -fsanitize=fuzzer)
endif()
```

The fuzzer entry point (`fuzz/cmark_fuzz.c`) implements:
```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Parse data as CommonMark
  // Render to all formats
  // Free everything
  // Return 0
}
```

This subjects all parsers and renderers to random input.

### Building with Fuzzing

```bash
cmake -DCMARK_LIB_FUZZER=ON \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_C_FLAGS="-fsanitize=fuzzer-no-link,address" \
      ..
make
./cmark_fuzz corpus/
```

## Sanitizer Builds

### Address Sanitizer (ASan)

```bash
cmake -DCMAKE_BUILD_TYPE=Asan ..
```

Sets flags: `-fsanitize=address -fno-omit-frame-pointer`

Detects:
- Buffer overflows (stack, heap, global)
- Use-after-free
- Double-free
- Memory leaks (LSAN)

### Undefined Behavior Sanitizer (UBSan)

```bash
cmake -DCMAKE_BUILD_TYPE=Ubsan ..
```

Sets flags: `-fsanitize=undefined`

Detects:
- Signed integer overflow
- Null pointer dereference
- Misaligned access
- Invalid shift
- Out-of-bounds array access

## Running Tests

### Full Test Suite

```bash
mkdir build && cd build
cmake ..
make
ctest
```

### Verbose Output

```bash
ctest --verbose
```

### Single Test

```bash
ctest -R api_test
ctest -R spec_test
```

### With ASan

```bash
mkdir build-asan && cd build-asan
cmake -DCMAKE_BUILD_TYPE=Asan ..
make
ctest
```

## Test Data Files

| File | Purpose |
|------|---------|
| `test/spec.txt` | CommonMark specification with examples |
| `test/smart_punct.txt` | Smart punctuation examples |
| `test/entity.txt` | HTML entity test cases |
| `test/regression.txt` | Regression test cases |
| `test/spec_tests.py` | Spec test runner script |
| `test/pathological_tests.py` | Pathological input tests |
| `test/roundtrip_tests.py` | CommonMark roundtrip tests |
| `api_test/main.c` | C API unit tests |
| `fuzz/cmark_fuzz.c` | LibFuzzer entry point |

## Cross-References

- [building.md](building.md) — Build configurations including test builds
- [public-api.md](public-api.md) — API functions tested by `api_test`
- [cli-usage.md](cli-usage.md) — The `cmark` binary tested by spec tests
