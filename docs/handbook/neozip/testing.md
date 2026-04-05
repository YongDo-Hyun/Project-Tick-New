# Testing

## Overview

Neozip has a comprehensive test suite covering correctness, fuzz testing,
performance benchmarking, and regression testing for known CVEs. Testing
is built with CMake (`BUILD_TESTING=ON`, default) and uses Google Test
for structured test cases.

---

## Build Configuration

```cmake
option(BUILD_TESTING "Build test binaries" ON)
option(WITH_GTEST "Build with GTest" ON)
option(WITH_FUZZERS "Build fuzz targets" OFF)
option(WITH_BENCHMARKS "Build benchmarks" OFF)
option(WITH_SANITIZER "Build with sanitizer" OFF)
```

### Building Tests

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON -DWITH_GTEST=ON
cmake --build .
ctest --output-on-failure
```

### With Sanitizers

```bash
cmake .. -DWITH_SANITIZER=address    # ASan
cmake .. -DWITH_SANITIZER=memory     # MSan
cmake .. -DWITH_SANITIZER=undefined  # UBSan
cmake .. -DWITH_SANITIZER=thread     # TSan
```

### With Code Coverage

```bash
cmake .. -DWITH_CODE_COVERAGE=ON
cmake --build .
ctest
# Generate coverage report
```

---

## Test Structure

Tests reside in the `test/` directory:

### Google Test Files

| File | What It Tests |
|---|---|
| `test_adler32.cc` | Adler-32 correctness and edge cases |
| `test_compare256.cc` | String comparison implementations |
| `test_compress.cc` | compress/uncompress one-shot API |
| `test_compress_bound.cc` | compressBound accuracy |
| `test_crc32.cc` | CRC-32 correctness |
| `test_cve.cc` | CVE regression tests |
| `test_deflate_bound.cc` | deflateBound accuracy |
| `test_deflate_copy.cc` | deflateCopy correctness |
| `test_deflate_dict.cc` | Dictionary-based compression |
| `test_deflate_hash_head_0.cc` | Hash table edge case |
| `test_deflate_header.cc` | Gzip header handling |
| `test_deflate_params.cc` | Dynamic level/strategy changes |
| `test_deflate_pending.cc` | deflatePending correctness |
| `test_deflate_prime.cc` | deflatePrime bit injection |
| `test_deflate_quick_bi_valid.cc` | Quick deflate bi_valid edge case |
| `test_deflate_tune.cc` | deflateTune parameter modification |
| `test_dict.cc` | Dictionary compression/decompression |
| `test_inflate_adler32.cc` | Inflate Adler-32 validation |
| `test_inflate_sync.cc` | inflateSync recovery |
| `test_infcover.cc` | Inflate code coverage |
| `test_large_buffers.cc` | Large buffer handling |
| `test_main.cc` | Test runner entry point |
| `test_version.cc` | Version string checks |

### Standalone Test Utilities

| File | Purpose |
|---|---|
| `minigzip.c` | Minimal gzip compressor/decompressor |
| `minideflate.c` | Minimal deflate stream tool |
| `infcover.c` | Inflate code coverage driver |
| `switchlevels.c` | Test dynamic level switching |

### Test Data

The `test/data/` directory contains test vectors:

- Compressed files at various levels
- Known-good decompression outputs
- Edge-case inputs (empty, single-byte, very large)

---

## Google Test Details

### Test Fixture Pattern

Tests use parameterised fixtures for systematic coverage:

```cpp
class CompressTest : public testing::TestWithParam<std::tuple<int, int>> {
    // param<0> = compression level (0-9)
    // param<1> = strategy
};

TEST_P(CompressTest, RoundTrip) {
    auto [level, strategy] = GetParam();
    z_stream strm = {};
    deflateInit2(&strm, level, Z_DEFLATED, 15, 8, strategy);
    // Compress → decompress → verify
}

INSTANTIATE_TEST_SUITE_P(AllLevels, CompressTest,
    testing::Combine(
        testing::Range(0, 10),
        testing::Values(Z_DEFAULT_STRATEGY, Z_FILTERED, Z_HUFFMAN_ONLY,
                        Z_RLE, Z_FIXED)));
```

### Adler-32 Tests

```cpp
TEST(Adler32, KnownVectors) {
    // Test against known Adler-32 values
    uint32_t adler = adler32(0L, Z_NULL, 0);
    EXPECT_EQ(adler, 1U);

    adler = adler32(adler, (const uint8_t *)"Hello", 5);
    // Verify against expected value
}

TEST(Adler32, Combine) {
    // Verify adler32_combine produces correct results
    uint32_t a1 = adler32(0L, buf1, len1);
    uint32_t a2 = adler32(0L, buf2, len2);
    uint32_t combined = adler32_combine(a1, a2, len2);
    uint32_t full = adler32(0L, full_buf, len1 + len2);
    EXPECT_EQ(combined, full);
}
```

### CRC-32 Tests

```cpp
TEST(CRC32, KnownVectors) {
    uint32_t crc = crc32(0L, Z_NULL, 0);
    EXPECT_EQ(crc, 0U);

    crc = crc32(crc, (const uint8_t *)"123456789", 9);
    EXPECT_EQ(crc, 0xCBF43926U);  // Standard test vector
}
```

### CVE Regression Tests

`test_cve.cc` ensures previously discovered vulnerabilities remain fixed:

```cpp
TEST(CVE, TestHeapOverflow) {
    // Reproduce specific malformed input that triggered a vulnerability
    // Verify inflate returns Z_DATA_ERROR instead of crashing
    z_stream strm = {};
    inflateInit2(&strm, -15);
    strm.next_in = malformed_data;
    strm.avail_in = sizeof(malformed_data);
    strm.next_out = output;
    strm.avail_out = sizeof(output);
    int ret = inflate(&strm, Z_NO_FLUSH);
    EXPECT_EQ(ret, Z_DATA_ERROR);
    inflateEnd(&strm);
}
```

---

## Fuzz Testing

Fuzz targets are enabled with `-DWITH_FUZZERS=ON` and require a
fuzzing-capable compiler (Clang with libFuzzer or AFL):

### Fuzz Targets

| File | Target |
|---|---|
| `test/fuzz/fuzzer_compress.c` | compress/uncompress round-trip |
| `test/fuzz/fuzzer_deflate.c` | Deflate streaming API |
| `test/fuzz/fuzzer_inflate.c` | Inflate with arbitrary input |
| `test/fuzz/fuzzer_checksum.c` | Adler-32 and CRC-32 |
| `test/fuzz/fuzzer_gzip.c` | Gzip file I/O |

### Running Fuzzers

```bash
cmake .. -DWITH_FUZZERS=ON -DCMAKE_C_COMPILER=clang \
         -DCMAKE_C_FLAGS="-fsanitize=fuzzer-no-link,address"
cmake --build .

# Run a fuzzer
./fuzzer_inflate corpus/ -max_total_time=3600
```

Fuzz testing with AddressSanitizer catches:
- Buffer overflows/underflows
- Use-after-free
- Double-free
- Stack buffer overflows

---

## Benchmarks

Enabled with `-DWITH_BENCHMARKS=ON`:

### Benchmark Targets

| File | What It Benchmarks |
|---|---|
| `test/benchmarks/benchmark_adler32.cc` | Adler-32 throughput |
| `test/benchmarks/benchmark_compare256.cc` | String comparison throughput |
| `test/benchmarks/benchmark_crc32.cc` | CRC-32 throughput |
| `test/benchmarks/benchmark_compress.cc` | Compression throughput per level |
| `test/benchmarks/benchmark_inflate.cc` | Decompression throughput |
| `test/benchmarks/benchmark_slidehash.cc` | Hash table slide throughput |

Uses Google Benchmark framework:

```cpp
static void BM_Adler32(benchmark::State& state) {
    std::vector<uint8_t> data(state.range(0));
    for (auto _ : state) {
        adler32(1, data.data(), data.size());
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_Adler32)->Range(64, 1 << 20);
```

### Running Benchmarks

```bash
cmake .. -DWITH_BENCHMARKS=ON
cmake --build .
./benchmark_adler32
./benchmark_crc32
./benchmark_compress --benchmark_filter=".*level6.*"
```

---

## `minigzip` — Integration Test Tool

`test/minigzip.c` is a minimal gzip-compatible utility for manual testing:

```bash
# Compress
./minigzip < input.txt > input.txt.gz

# Decompress
./minigzip -d < input.txt.gz > output.txt

# Verify
diff input.txt output.txt
```

Options:
- `-d` — Decompress mode
- `-1` to `-9` — Compression level
- `-f` — Z_FILTERED strategy
- `-h` — Z_HUFFMAN_ONLY strategy
- `-R` — Z_RLE strategy
- `-F` — Z_FIXED strategy

---

## `minideflate` — Raw Deflate Test Tool

`test/minideflate.c` tests raw deflate streams (no wrapper):

```bash
./minideflate -c -k < input > compressed
./minideflate -d -k < compressed > output
```

---

## Running the Full Test Suite

```bash
cd build
ctest --output-on-failure -j$(nproc)
```

Individual tests can be run:
```bash
ctest -R test_adler32
ctest -R test_crc32
ctest -R test_compress
ctest -R test_cve
```

### CI Integration

The project uses CI for:
- Multiple compiler versions (GCC, Clang, MSVC)
- Multiple architectures (x86_64, AArch64, Power, s390x)
- Multiple configurations (compat mode, native mode, sanitizers)
- Multiple operating systems (Linux, macOS, Windows)

Test results are reported via CTest and Google Test XML output.
