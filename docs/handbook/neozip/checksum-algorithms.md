# Checksum Algorithms

## Overview

Neozip implements two checksum algorithms used by the DEFLATE family of
compression formats:

- **Adler-32**: A fast checksum used in the zlib container (RFC 1950)
- **CRC-32**: A more robust check used in the gzip container (RFC 1952)

Both algorithms have SIMD-accelerated implementations across x86, ARM,
Power, RISC-V, s390, and LoongArch architectures.

---

## Adler-32

### Algorithm

Adler-32 is defined in RFC 1950. It consists of two running sums:

- **s1**: Sum of all bytes (mod BASE)
- **s2**: Sum of all intermediate s1 values (mod BASE)

```c
#define BASE 65521U    // Largest prime less than 65536
#define NMAX 5552      // Largest n where 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32 - 1
```

The `NMAX` constant determines how many bytes can be accumulated before a
modular reduction is required to prevent 32-bit overflow.

### Scalar Implementation

From `adler32.c`:

```c
Z_INTERNAL uint32_t adler32_c(uint32_t adler, const uint8_t *buf, size_t len) {
    uint32_t sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    if (len == 1) {
        adler += buf[0];
        if (adler >= BASE) adler -= BASE;
        sum2 += adler;
        if (sum2 >= BASE) sum2 -= BASE;
        return adler | (sum2 << 16);
    }

    // Split into NMAX-sized blocks
    while (len >= NMAX) {
        len -= NMAX;
        unsigned n = NMAX / 16;
        do {
            // Unrolled: 16 ADLER_DO per iteration
            ADLER_DO16(buf);
            buf += 16;
        } while (--n);
        MOD(adler);      // adler %= BASE
        MOD(sum2);
    }

    // Process remaining bytes
    while (len >= 16) {
        len -= 16;
        ADLER_DO16(buf);
        buf += 16;
    }
    while (len--) {
        adler += *buf++;
        sum2 += adler;
    }
    MOD(adler);
    MOD(sum2);
    return adler | (sum2 << 16);
}
```

### Accumulation Macros

From `adler32_p.h`:

```c
#define ADLER_DO1(buf)  { adler += *(buf); sum2 += adler; }
#define ADLER_DO2(buf)  ADLER_DO1(buf); ADLER_DO1(buf + 1)
#define ADLER_DO4(buf)  ADLER_DO2(buf); ADLER_DO2(buf + 2)
#define ADLER_DO8(buf)  ADLER_DO4(buf); ADLER_DO4(buf + 4)
#define ADLER_DO16(buf) ADLER_DO8(buf); ADLER_DO8(buf + 8)
```

### Modular Reduction

```c
#define MOD(a) a %= BASE
#define MOD4(a) a %= BASE
```

The straightforward modulo works well because BASE is prime. On architectures
where division is expensive, Adler-32 can alternatively be reduced by
subtracting BASE in a loop.

### Combining Adler-32 Checksums

`adler32_combine_()` merges two Adler-32 checksums from adjacent data
segments without accessing the original data:

```c
static uint32_t adler32_combine_(uint32_t adler1, uint32_t adler2, z_off64_t len2) {
    uint32_t sum1, sum2;
    unsigned rem;

    // modular arithmetic to combine:
    // s1_combined = (s1_a + s1_b - 1) % BASE
    // s2_combined = (s2_a + s2_b + s1_a * len2 - len2) % BASE
    rem = (unsigned)(len2 % BASE);
    sum1 = adler1 & 0xffff;
    sum2 = rem * sum1;
    MOD(sum2);
    sum1 += (adler2 & 0xffff) + BASE - 1;
    sum2 += ((adler1 >> 16) & 0xffff) + ((adler2 >> 16) & 0xffff) + BASE - rem;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum2 >= ((unsigned long)BASE << 1)) sum2 -= ((unsigned long)BASE << 1);
    if (sum2 >= BASE) sum2 -= BASE;
    return sum1 | (sum2 << 16);
}
```

### SIMD Implementations

SIMD Adler-32 uses parallel accumulation with dot products:

**AVX2** (`arch/x86/adler32_avx2.c`):
```c
Z_INTERNAL uint32_t adler32_avx2(uint32_t adler, const uint8_t *buf, size_t len) {
    static const uint8_t dot2v[] = {32,31,30,...,1};   // Position weights
    static const uint8_t dot3v[] = {32,32,32,...,32};  // Sum1 weight (all ones)
    __m256i vbuf, vs1, vs2, vs1_0, vs3;

    vs1 = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, adler & 0xffff);
    vs2 = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, adler >> 16);
    vs1_0 = vs1;

    while (len >= 32) {
        vs1_0 = vs1;
        // Load 32 bytes
        vbuf = _mm256_loadu_si256((__m256i*)buf);
        // sum1 += bytes[0..31]
        vs1 = _mm256_add_epi32(vs1, _mm256_sad_epu8(vbuf, _mm256_setzero_si256()));
        // vs3 = dot product: sum2 += (32-i)*byte[i]
        vs3 = _mm256_maddubs_epi16(vbuf, vdot2v);
        // ... accumulate vs2
        vs2 = _mm256_add_epi32(vs2, _mm256_madd_epi16(vs3, vones));
        // vs2 += 32 * previous_vs1
        vs2 = _mm256_add_epi32(vs2, _mm256_slli_epi32(vs1_0, 5));
        buf += 32;
        len -= 32;
    }
    // Horizontal reduction and modular reduction
    ...
}
```

The key insight: Instead of computing `sum2 += s1_n` for each byte n
individually, SIMD computes `sum2 += k * byte[i]` via `_mm256_maddubs_epi16()`
where k represents the positional weight.

**Available SIMD variants**:

| Architecture | Implementation | Vector Width |
|---|---|---|
| x86 SSE4.1 | `adler32_sse41.c` | 128-bit |
| x86 SSSE3 | `adler32_ssse3.c` | 128-bit |
| x86 AVX2 | `adler32_avx2.c` | 256-bit |
| x86 AVX-512 | `adler32_avx512.c` | 512-bit |
| x86 AVX-512+VNNI | `adler32_avx512_vnni.c` | 512-bit |
| ARM NEON | `adler32_neon.c` | 128-bit |
| Power VMX (Altivec) | `adler32_vmx.c` | 128-bit |
| Power8 | `adler32_power8.c` | 128-bit |
| RISC-V RVV | `adler32_rvv.c` | Scalable |
| LoongArch LASX | `adler32_lasx.c` | 256-bit |

### Adler-32 with Copy

`adler32_copy()` computes Adler-32 while simultaneously copying data,
fusing two memory passes into one:

```c
typedef uint32_t (*adler32_copy_func)(uint32_t adler, uint8_t *dst,
                                       const uint8_t *src, size_t len);
```

This is used during inflate to compute the checksum while copying
decompressed data to the output buffer.

---

## CRC-32

### Algorithm

CRC-32 uses the standard polynomial 0xEDB88320 (reflected form):

```c
#define POLY 0xedb88320     // CRC-32 polynomial (reversed)
```

### Braided CRC-32

The default software implementation uses a "braided" algorithm that
processes multiple bytes per step using interleaved CRC tables:

```c
#define BRAID_N 5           // Number of interleaved CRC computations
#define BRAID_W 8           // Bytes per word (8 for 64-bit, 4 for 32-bit)
```

From `crc32_braid_p.h`, the braided approach processes 5 words (40 bytes
on 64-bit) per iteration:

```c
// Braided CRC processing (conceptual)
// Process BRAID_N words at a time:
z_word_t braids[BRAID_N];

// Load BRAID_N words from input
for (int k = 0; k < BRAID_N; k++)
    braids[k] = *(z_word_t *)(buf + k * BRAID_W);

// For each word, XOR with running CRC then look up table
for (int k = 0; k < BRAID_N; k++) {
    z_word_t word = braids[k];
    // CRC-fold using braid tables:
    // crc = crc_braid_table[N-1-k][byte0] ^ ... ^ crc_braid_table[0][byteN-1]
}
```

The braid tables are generated at compile time by `crc32_braid_tbl.h`.

### Chorba CRC-32

A newer CRC-32 algorithm using a "Chorba" reduction technique for
even faster software CRC computation. Selected when size >= 256 bytes:

```c
Z_INTERNAL uint32_t crc32_braid(uint32_t crc, const uint8_t *buf, size_t len) {
    // Short paths for small inputs
    if (len < 64) {
        return crc32_small(crc, buf, len);
    }
    // For lengths >= threshold, use Chorba
    if (len >= 256) {
        return crc32_chorba(crc, buf, len);
    }
    // Otherwise use braided
    ...
}
```

### SIMD CRC-32 Implementations

Hardware-accelerated CRC-32 is available on these architectures:

| Architecture | Instruction | File |
|---|---|---|
| x86 (PCLMULQDQ) | Carry-less multiply | `crc32_pclmulqdq.c` |
| x86 (VPCLMULQDQ) | AVX-512 carry-less multiply | `crc32_vpclmulqdq.c` |
| ARM (CRC32) | CRC32W/CRC32B instructions | `crc32_acle.c` |
| ARM (PMULL) | Polynomial multiply long | `crc32_pmull.c` |
| Power8 | Vector carry-less multiply | `crc32_power8.c` |
| s390 (CRC32) | DFLTCC or hardware CRC | `crc32_vx.c` |
| RISC-V | Zbc carry-less multiply | `crc32_rvv.c` |

**x86 PCLMULQDQ** (`arch/x86/crc32_pclmulqdq.c`):
Uses Barrett reduction via carry-less multiplication to fold 64 bytes at
a time:

```c
Z_INTERNAL uint32_t crc32_pclmulqdq(uint32_t crc32, const uint8_t *buf, size_t len) {
    __m128i xmm_crc0, xmm_crc1, xmm_crc2, xmm_crc3;
    __m128i xmm_fold4;   // Fold constant

    // Initialize with CRC and first 64 bytes
    xmm_crc0 = _mm_loadu_si128((__m128i *)buf);
    xmm_crc0 = _mm_xor_si128(xmm_crc0, _mm_cvtsi32_si128(crc32));
    // ... load crc1, crc2, crc3

    // Main fold loop: process 64 bytes per iteration
    while (len >= 64) {
        // Fold: crc_n = pclmulqdq(crc_n, fold_constant) ^ next_data
        xmm_crc0 = _mm_xor_si128(
            _mm_clmulepi64_si128(xmm_crc0, xmm_fold4, 0x01),
            _mm_clmulepi64_si128(xmm_crc0, xmm_fold4, 0x10));
        xmm_crc0 = _mm_xor_si128(xmm_crc0, _mm_loadu_si128(next++));
        // Repeat for crc1..crc3
    }

    // Final reduction to 32-bit CRC
    // Barrett reduction using mu and polynomial constants
}
```

This processes data at ~16 bytes/cycle on modern x86 hardware.

### CRC-32 with Copy

Like Adler-32, CRC-32 has a combined compute-and-copy variant:

```c
typedef uint32_t (*crc32_copy_func)(uint32_t crc, uint8_t *dst,
                                     const uint8_t *src, size_t len);
```

This fuses the CRC computation with the `memcpy`, utilising cache lines
loaded for copying to also feed the CRC calculation.

### Combining CRC-32 Values

```c
uint32_t crc32_combine(uint32_t crc1, uint32_t crc2, z_off_t len2);
uint32_t crc32_combine_gen(z_off_t len2);
uint32_t crc32_combine_op(uint32_t crc1, uint32_t crc2, uint32_t op);
```

Two-phase combine enables pre-computing the combination operator for a
known second-segment length, then applying it to multiple CRC pairs.

---

## Dispatch via `functable`

Checksum functions are dispatched through the `functable_s` structure:

```c
struct functable_s {
    adler32_func          adler32;
    adler32_copy_func     adler32_copy;
    compare256_func       compare256;
    crc32_func            crc32;
    crc32_copy_func       crc32_copy;
    // ... other function pointers
};
```

`functable.c` selects the best implementation at runtime:

```c
// x86 dispatch cascade for adler32:
#ifdef X86_SSE42
    if (cf.x86.has_sse42)
        functable.adler32 = adler32_sse42;
#endif
#ifdef X86_AVX2
    if (cf.x86.has_avx2)
        functable.adler32 = adler32_avx2;
#endif
#ifdef X86_AVX512
    if (cf.x86.has_avx512)
        functable.adler32 = adler32_avx512;
#endif
#ifdef X86_AVX512VNNI
    if (cf.x86.has_avx512vnni)
        functable.adler32 = adler32_avx512_vnni;
#endif
```

Each architecture-specific source file is compiled separately with its
required SIMD flags (e.g., `-mavx2`, `-mpclmul`).

---

## Function Table API

### Public API

```c
uint32_t PREFIX(adler32)(uint32_t adler, const uint8_t *buf, uint32_t len);
uint32_t PREFIX(crc32)(uint32_t crc, const uint8_t *buf, uint32_t len);
```

For zlib compatibility, `adler32_z()` and `crc32_z()` accept `size_t` length:

```c
uint32_t PREFIX(adler32_z)(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t PREFIX(crc32_z)(uint32_t crc, const uint8_t *buf, size_t len);
```

### Initial Values

- Adler-32: `adler32(0, NULL, 0)` returns `1` (initial value)
- CRC-32: `crc32(0, NULL, 0)` returns `0` (initial value)

### Typical Usage

```c
uint32_t checksum = PREFIX(adler32)(0L, Z_NULL, 0);
checksum = PREFIX(adler32)(checksum, data, data_len);
// checksum now holds the Adler-32 of data[0..data_len-1]
```

---

## Performance Characteristics

### Adler-32

| Implementation | Throughput (approximate) |
|---|---|
| Scalar C | ~1 byte/cycle |
| SSE4.1 | ~8 bytes/cycle |
| AVX2 | ~16 bytes/cycle |
| AVX-512+VNNI | ~32 bytes/cycle |
| ARM NEON | ~8 bytes/cycle |

### CRC-32

| Implementation | Throughput (approximate) |
|---|---|
| Braided (scalar) | ~4 bytes/cycle |
| PCLMULQDQ | ~16 bytes/cycle |
| VPCLMULQDQ (AVX-512) | ~64 bytes/cycle |
| ARM CRC32 | ~4 bytes/cycle |
| ARM PMULL | ~16 bytes/cycle |

CRC-32 is computationally heavier than Adler-32, but hardware acceleration
closes the gap significantly.

---

## Checksum in the Compression Pipeline

### During Deflate

In `deflate.c`, checksums are computed on the input data:

```c
if (s->wrap == 2) {
    // gzip: CRC-32
    strm->adler = FUNCTABLE_CALL(crc32)(strm->adler, strm->next_in, strm->avail_in);
} else if (s->wrap == 1) {
    // zlib: Adler-32
    strm->adler = FUNCTABLE_CALL(adler32)(strm->adler, strm->next_in, strm->avail_in);
}
```

### During Inflate

In `inflate.c`, checksums are computed on the output data:

```c
static inline void inf_chksum(PREFIX3(stream) *strm, const uint8_t *buf, uint32_t len) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    if (state->flags)
        strm->adler = state->check = FUNCTABLE_CALL(crc32)(state->check, buf, len);
    else
        strm->adler = state->check = FUNCTABLE_CALL(adler32)(state->check, buf, len);
}
```

The `_copy` variants (`inf_chksum_cpy`) are preferred when data is being
both checksummed and copied, as they fuse the two operations.
