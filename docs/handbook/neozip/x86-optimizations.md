# x86 Optimizations

## Overview

Neozip provides extensive x86 SIMD optimizations spanning SSE2, SSSE3,
SSE4.1, SSE4.2, PCLMULQDQ, AVX2, AVX-512, AVX-512+VNNI, and VPCLMULQDQ.
All implementations live in `arch/x86/` and are selected at runtime by
`functable.c` based on CPUID detection.

---

## Source Files

| File | ISA | Function |
|---|---|---|
| `x86_features.c/h` | — | CPUID feature detection |
| `adler32_avx2.c` | AVX2 | Adler-32 checksum |
| `adler32_avx512.c` | AVX-512 | Adler-32 checksum |
| `adler32_avx512_vnni.c` | AVX-512+VNNI | Adler-32 checksum |
| `adler32_sse42.c` | SSE4.2 | Adler-32 checksum |
| `adler32_ssse3.c` | SSSE3 | Adler-32 checksum |
| `crc32_pclmulqdq.c` | PCLMULQDQ | CRC-32 (carry-less multiply) |
| `crc32_vpclmulqdq.c` | VPCLMULQDQ | CRC-32 (AVX-512 CLMUL) |
| `compare256_avx2.c` | AVX2 | 256-byte comparison |
| `compare256_sse2.c` | SSE2 | 256-byte comparison |
| `compare256_sse42.c` | SSE4.2 | 256-byte comparison |
| `chunkset_avx2.c` | AVX2 | Pattern fill for inflate |
| `chunkset_sse2.c` | SSE2 | Pattern fill for inflate |
| `slide_hash_avx2.c` | AVX2 | Hash table slide |
| `slide_hash_avx512.c` | AVX-512 | Hash table slide |
| `slide_hash_sse2.c` | SSE2 | Hash table slide |
| `insert_string_sse42.c` | SSE4.2 | CRC-based hash insertion |
| `inffast_avx2.c` | AVX2 | Fast inflate inner loop |
| `inffast_sse2.c` | SSE2 | Fast inflate inner loop |

---

## Feature Detection

### CPUID Queries

`x86_features.c` queries CPUID leaves 1 and 7:

```c
void Z_INTERNAL x86_check_features(struct cpu_features *features) {
    unsigned eax, ebx, ecx, edx;

    // Leaf 1 — basic features
    cpuid(1, &eax, &ebx, &ecx, &edx);
    features->x86.has_sse2       = !!(edx & (1 << 26));
    features->x86.has_ssse3      = !!(ecx & (1 << 9));
    features->x86.has_sse41      = !!(ecx & (1 << 19));
    features->x86.has_sse42      = !!(ecx & (1 << 20));
    features->x86.has_pclmulqdq  = !!(ecx & (1 << 1));

    // Check OS YMM/ZMM support via XSAVE/XGETBV
    if (ecx & (1 << 27)) {
        uint64_t xcr0 = xgetbv(0);
        features->x86.has_os_save_ymm = ((xcr0 & 0x06) == 0x06);
        features->x86.has_os_save_zmm = ((xcr0 & 0xe6) == 0xe6);
    }

    // Leaf 7, sub-leaf 0 — extended features
    cpuidp(7, 0, &eax, &ebx, &ecx, &edx);
    if (features->x86.has_os_save_ymm)
        features->x86.has_avx2 = !!(ebx & (1 << 5));
    if (features->x86.has_os_save_zmm) {
        features->x86.has_avx512f   = !!(ebx & (1 << 16));
        features->x86.has_avx512dq  = !!(ebx & (1 << 17));
        features->x86.has_avx512bw  = !!(ebx & (1 << 30));
        features->x86.has_avx512vl  = !!(ebx & (1 << 31));
        features->x86.has_vpclmulqdq = !!(ecx & (1 << 10));
        features->x86.has_avx512vnni = !!(ecx & (1 << 11));
    }
    features->x86.has_avx512_common =
        features->x86.has_avx512f && features->x86.has_avx512dq &&
        features->x86.has_avx512bw && features->x86.has_avx512vl;
}
```

### `xgetbv()` — Reading Extended Control Register

```c
static inline uint64_t xgetbv(unsigned xcr) {
    uint32_t eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
    return ((uint64_t)edx << 32) | eax;
}
```

This verifies the OS has enabled the save/restore of wider register files.
Without this check, using YMM/ZMM registers would cause a #UD fault.

---

## Adler-32 Implementations

### SSSE3 (`adler32_ssse3.c`)

Uses `_mm_maddubs_epi16` for weighted position sums on 16-byte vectors:

```c
Z_INTERNAL uint32_t adler32_ssse3(uint32_t adler, const uint8_t *buf, size_t len) {
    __m128i vs1 = _mm_cvtsi32_si128(adler & 0xffff);
    __m128i vs2 = _mm_cvtsi32_si128(adler >> 16);
    const __m128i dot2v = _mm_setr_epi8(16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1);

    while (len >= 16) {
        __m128i vbuf = _mm_loadu_si128((__m128i *)buf);
        // sum1 += bytes
        vs1 = _mm_add_epi32(vs1, _mm_sad_epu8(vbuf, _mm_setzero_si128()));
        // sum2 += position_weight * bytes
        __m128i vtmp = _mm_maddubs_epi16(vbuf, dot2v);
        vs2 = _mm_add_epi32(vs2, _mm_madd_epi16(vtmp, _mm_set1_epi16(1)));
        // Accumulate 16 * prev_s1 into s2
        vs2 = _mm_add_epi32(vs2, _mm_slli_epi32(vs1_0, 4));
        buf += 16;
        len -= 16;
    }
    // Horizontal reduction and MOD BASE
}
```

### AVX2 (`adler32_avx2.c`)

Processes 32 bytes per iteration using 256-bit registers:

```c
Z_INTERNAL uint32_t adler32_avx2(uint32_t adler, const uint8_t *buf, size_t len) {
    static const uint8_t dot2v_data[] = {32,31,30,...,2,1};
    __m256i vdot2v = _mm256_loadu_si256((__m256i*)dot2v_data);
    __m256i vs1 = _mm256_set_epi32(0,0,0,0,0,0,0, adler & 0xffff);
    __m256i vs2 = _mm256_set_epi32(0,0,0,0,0,0,0, adler >> 16);

    while (len >= 32) {
        __m256i vbuf = _mm256_loadu_si256((__m256i *)buf);
        // s1 += sum of all bytes (using SAD against zero)
        vs1 = _mm256_add_epi32(vs1,
            _mm256_sad_epu8(vbuf, _mm256_setzero_si256()));
        // s2 += weighted sum (dot product approach)
        __m256i vtmp = _mm256_maddubs_epi16(vbuf, vdot2v);
        vs2 = _mm256_add_epi32(vs2,
            _mm256_madd_epi16(vtmp, _mm256_set1_epi16(1)));
        // s2 += 32 * prev_s1
        vs2 = _mm256_add_epi32(vs2, _mm256_slli_epi32(vs1_0, 5));
        buf += 32;
        len -= 32;
    }
}
```

The `_mm256_maddubs_epi16` instruction multiplies unsigned bytes by signed
bytes and sums adjacent pairs, computing the weighted position sum in one
instruction. `_mm256_sad_epu8` computes the horizontal sum of bytes.

### AVX-512 (`adler32_avx512.c`)

Processes 64 bytes per iteration using 512-bit `__m512i` registers:

```c
__m512i vs1 = _mm512_set_epi32(0,...,0, adler & 0xffff);
__m512i vs2 = _mm512_set_epi32(0,...,0, adler >> 16);

while (len >= 64) {
    __m512i vbuf = _mm512_loadu_si512(buf);
    vs1 = _mm512_add_epi32(vs1, _mm512_sad_epu8(vbuf, _mm512_setzero_si512()));
    __m512i vtmp = _mm512_maddubs_epi16(vbuf, vdot2v);
    vs2 = _mm512_add_epi32(vs2, _mm512_madd_epi16(vtmp, vones));
    vs2 = _mm512_add_epi32(vs2, _mm512_slli_epi32(vs1_0, 6));
    buf += 64;
    len -= 64;
}
```

### AVX-512+VNNI (`adler32_avx512_vnni.c`)

Uses `_mm512_dpbusd_epi32` (dot product of unsigned bytes and signed bytes),
available with the VNNI extension:

```c
// VPDPBUSD replaces maddubs + madd sequence with a single instruction
vs2 = _mm512_dpbusd_epi32(vs2, vbuf, vdot2v);
```

---

## CRC-32 Implementations

### PCLMULQDQ (`crc32_pclmulqdq.c`)

Uses carry-less multiplication for CRC folding. Processes 64 bytes per
iteration with four XMM accumulators:

```c
Z_INTERNAL uint32_t crc32_pclmulqdq(uint32_t crc, const uint8_t *buf, size_t len) {
    __m128i xmm_crc0, xmm_crc1, xmm_crc2, xmm_crc3;
    __m128i xmm_fold4 = _mm_set_epi32(0x00000001, 0x54442bd4,
                                        0x00000001, 0xc6e41596);

    // Init: XOR CRC into first 16 bytes of data
    xmm_crc0 = _mm_xor_si128(_mm_loadu_si128(buf), _mm_cvtsi32_si128(crc));
    xmm_crc1 = _mm_loadu_si128(buf + 16);
    xmm_crc2 = _mm_loadu_si128(buf + 32);
    xmm_crc3 = _mm_loadu_si128(buf + 48);

    // Main loop: fold 64 bytes per iteration
    while (len >= 64) {
        // For each accumulator:
        // crc_n = clmul(crc_n, fold4, 0x01) ^ clmul(crc_n, fold4, 0x10) ^ next_data
        __m128i xmm_t0 = _mm_clmulepi64_si128(xmm_crc0, xmm_fold4, 0x01);
        __m128i xmm_t1 = _mm_clmulepi64_si128(xmm_crc0, xmm_fold4, 0x10);
        xmm_crc0 = _mm_xor_si128(_mm_xor_si128(xmm_t0, xmm_t1),
                                   _mm_loadu_si128(next++));
        // repeat for crc1..crc3
    }

    // Fold 4→1, then Barrett reduction to 32-bit CRC
    // ...
}
```

### VPCLMULQDQ (`crc32_vpclmulqdq.c`)

Uses AVX-512 carry-less multiply to process 256 bytes per iteration
with four ZMM (512-bit) accumulators:

```c
__m512i zmm_crc0 = _mm512_loadu_si512(buf);
zmm_crc0 = _mm512_xor_si512(zmm_crc0, _mm512_castsi128_si512(_mm_cvtsi32_si128(crc)));
// ... 3 more accumulators

while (len >= 256) {
    __m512i zmm_t0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x01);
    __m512i zmm_t1 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x10);
    zmm_crc0 = _mm512_ternarylogic_epi64(zmm_t0, zmm_t1,
                                           _mm512_loadu_si512(next++), 0x96);
    // XOR three values in one instruction via ternarylogic
}
```

`_mm512_ternarylogic_epi64(..., 0x96)` computes `A ^ B ^ C` in a single
instruction, fusing two XOR operations.

---

## String Comparison (`compare256`)

### SSE2 (`compare256_sse2.c`)

```c
Z_INTERNAL uint32_t compare256_sse2(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;
    do {
        __m128i v0 = _mm_loadu_si128((__m128i *)(src0 + len));
        __m128i v1 = _mm_loadu_si128((__m128i *)(src1 + len));
        __m128i cmp = _mm_cmpeq_epi8(v0, v1);
        unsigned mask = (unsigned)_mm_movemask_epi8(cmp);
        if (mask != 0xffff) {
            // Find first mismatch
            return len + __builtin_ctz(~mask);
        }
        len += 16;
    } while (len < 256);
    return 256;
}
```

### AVX2 (`compare256_avx2.c`)

Same approach with 32-byte vectors:

```c
Z_INTERNAL uint32_t compare256_avx2(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;
    do {
        __m256i v0 = _mm256_loadu_si256((__m256i *)(src0 + len));
        __m256i v1 = _mm256_loadu_si256((__m256i *)(src1 + len));
        __m256i cmp = _mm256_cmpeq_epi8(v0, v1);
        unsigned mask = (unsigned)_mm256_movemask_epi8(cmp);
        if (mask != 0xffffffff) {
            return len + __builtin_ctz(~mask);
        }
        len += 32;
    } while (len < 256);
    return 256;
}
```

### SSE4.2 (`compare256_sse42.c`)

Uses `_mm_cmpistri` (string compare instruction):

```c
Z_INTERNAL uint32_t compare256_sse42(const uint8_t *src0, const uint8_t *src1) {
    // _mm_cmpistri with EQUAL_EACH | NEGATIVE_POLARITY finds first mismatch
    // in a 16-byte comparison
}
```

---

## Slide Hash

### SSE2 (`slide_hash_sse2.c`)

```c
Z_INTERNAL void slide_hash_sse2(deflate_state *s) {
    Pos *p;
    unsigned n;
    __m128i xmm_wsize = _mm_set1_epi16((uint16_t)s->w_size);

    n = HASH_SIZE;
    p = &s->head[n];
    do {
        p -= 8;
        __m128i value = _mm_loadu_si128((__m128i *)p);
        _mm_storeu_si128((__m128i *)p,
            _mm_subs_epu16(value, xmm_wsize));  // Saturating subtract
        n -= 8;
    } while (n);
    // Same for s->prev
}
```

### AVX-512 (`slide_hash_avx512.c`)

Processes 32 entries (64 bytes) per iteration:

```c
Z_INTERNAL void slide_hash_avx512(deflate_state *s) {
    __m512i zmm_wsize = _mm512_set1_epi16((uint16_t)s->w_size);
    // Process 32 uint16_t entries per iteration
    for (...) {
        __m512i v = _mm512_loadu_si512(p);
        _mm512_storeu_si512(p, _mm512_subs_epu16(v, zmm_wsize));
    }
}
```

---

## Hash Insertion (SSE4.2)

`insert_string_sse42.c` uses the hardware CRC32 instruction for hashing:

```c
Z_INTERNAL Pos insert_string_sse42(deflate_state *s,
                                    Pos str, unsigned count) {
    Pos idx;
    for (unsigned i = 0; i < count; i++) {
        unsigned val = *(uint32_t *)(s->window + str + i);
        uint32_t h = 0;
        h = _mm_crc32_u32(h, val);    // Hardware CRC32C
        h &= s->hash_mask;
        idx = s->head[h];
        s->prev[str + i & s->w_mask] = idx;
        s->head[h] = (Pos)(str + i);
    }
    return idx;
}
```

The CRC32C instruction provides excellent hash distribution with near-zero
cost.

---

## Chunkset (Inflate Copy)

### SSE2 (`chunkset_sse2.c`)

Used during inflate for back-reference copying:

```c
Z_INTERNAL uint8_t* chunkmemset_safe_sse2(uint8_t *out, uint8_t *from,
                                            unsigned dist, unsigned len) {
    if (dist >= 16) {
        // Standard copy with SSE2 loads/stores
        while (len >= 16) {
            _mm_storeu_si128((__m128i *)out, _mm_loadu_si128((__m128i *)from));
            out += 16;
            from += 16;
            len -= 16;
        }
    } else {
        // Replicate pattern: broadcast dist-byte pattern into 16 bytes
        // Handle dist=1 (memset), dist=2, dist=4, dist=8 specially
        __m128i pattern = replicate_pattern(from, dist);
        while (len >= 16) {
            _mm_storeu_si128((__m128i *)out, pattern);
            out += 16;
            len -= 16;
        }
    }
    return out;
}
```

### AVX2 (`chunkset_avx2.c`)

Same pattern with 32-byte chunks:

```c
// Replicate to 256-bit and store 32 bytes at a time
__m256i pattern = _mm256_broadcastsi128_si256(pattern_128);
while (len >= 32) {
    _mm256_storeu_si256((__m256i *)out, pattern);
    out += 32;
    len -= 32;
}
```

---

## CMake Configuration

Each x86 SIMD feature has a corresponding `WITH_` option:

```cmake
option(WITH_SSE2 "Build with SSE2" ON)
option(WITH_SSSE3 "Build with SSSE3" ON)
option(WITH_SSE42 "Build with SSE4.2" ON)
option(WITH_PCLMULQDQ "Build with PCLMULQDQ" ON)
option(WITH_AVX2 "Build with AVX2" ON)
option(WITH_AVX512 "Build with AVX-512" ON)
option(WITH_AVX512VNNI "Build with AVX512VNNI" ON)
option(WITH_VPCLMULQDQ "Build with VPCLMULQDQ" ON)
```

Each source file is compiled with its minimum required flags:

```cmake
set_property(SOURCE arch/x86/adler32_avx2.c APPEND PROPERTY COMPILE_OPTIONS -mavx2)
set_property(SOURCE arch/x86/crc32_pclmulqdq.c APPEND PROPERTY COMPILE_OPTIONS -mpclmul -msse4.2)
set_property(SOURCE arch/x86/crc32_vpclmulqdq.c APPEND PROPERTY COMPILE_OPTIONS -mvpclmulqdq -mavx512f)
```

This ensures the main code compiles without SIMD requirements while
individual acceleration files use their specific instruction sets.
