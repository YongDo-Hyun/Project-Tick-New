# ARM Optimizations

## Overview

Neozip provides ARM SIMD optimizations using NEON (Advanced SIMD), CRC32
hardware instructions, and PMULL (polynomial multiply long). These cover
both AArch32 (ARMv7+) and AArch64 (ARMv8+) targets. All implementations
reside in `arch/arm/`.

---

## Source Files

| File | ISA Extension | Function |
|---|---|---|
| `arm_features.c/h` | — | Feature detection |
| `adler32_neon.c` | NEON | Adler-32 checksum |
| `chunkset_neon.c` | NEON | Pattern fill for inflate |
| `compare256_neon.c` | NEON | 256-byte string comparison |
| `crc32_acle.c` | CRC32 | Hardware CRC-32 |
| `crc32_pmull.c` | PMULL | CLMUL-based CRC-32 |
| `insert_string_acle.c` | CRC32 | CRC-based hash insertion |
| `slide_hash_neon.c` | NEON | Hash table slide |
| `inffast_neon.c` | NEON | Fast inflate inner loop |

---

## Feature Detection

### `arm_cpu_features` Structure

```c
struct arm_cpu_features {
    int has_simd;        // ARMv6 SIMD (AArch32 only)
    int has_neon;        // NEON / ASIMD
    int has_crc32;       // CRC32 instructions (ARMv8.0-A optional, ARMv8.1-A mandatory)
    int has_pmull;       // PMULL (polynomial multiply long, 64→128-bit)
    int has_eor3;        // SHA3 EOR3 instruction (ARMv8.2-A+SHA3)
    int has_fast_pmull;  // High-perf PMULL
};
```

### Linux Detection

```c
void Z_INTERNAL arm_check_features(struct cpu_features *features) {
#if defined(__linux__)
    unsigned long hwcap = getauxval(AT_HWCAP);
#if defined(__aarch64__)
    features->arm.has_neon  = !!(hwcap & HWCAP_ASIMD);
    features->arm.has_crc32 = !!(hwcap & HWCAP_CRC32);
    features->arm.has_pmull = !!(hwcap & HWCAP_PMULL);
    unsigned long hwcap2 = getauxval(AT_HWCAP2);
    features->arm.has_eor3 = !!(hwcap2 & HWCAP2_SHA3);
#else  // AArch32
    features->arm.has_simd = !!(hwcap & HWCAP_ARM_VFPv3);
    features->arm.has_neon = !!(hwcap & HWCAP_ARM_NEON);
    features->arm.has_crc32 = !!(hwcap2 & HWCAP2_CRC32);
    features->arm.has_pmull = !!(hwcap2 & HWCAP2_PMULL);
#endif
#endif
}
```

### macOS/iOS Detection

```c
#if defined(__APPLE__)
    // NEON is always available on Apple Silicon
    features->arm.has_neon = 1;
    features->arm.has_crc32 = has_feature("hw.optional.armv8_crc32");
    features->arm.has_pmull = has_feature("hw.optional.arm.FEAT_PMULL");
#endif
```

### Windows Detection

```c
#if defined(_WIN32)
    features->arm.has_neon  = IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE);
    features->arm.has_crc32 = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
#endif
```

---

## NEON Adler-32 (`adler32_neon.c`)

Uses 128-bit NEON registers to process 16 bytes per iteration:

```c
Z_INTERNAL uint32_t adler32_neon(uint32_t adler, const uint8_t *buf, size_t len) {
    uint32_t s1 = adler & 0xffff;
    uint32_t s2 = adler >> 16;

    // Position weight vector: {16,15,14,...,1}
    static const uint8_t taps[] = {16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
    uint8x16_t vtaps = vld1q_u8(taps);

    while (len >= 16) {
        uint32x4_t vs1 = vdupq_n_u32(0);
        uint32x4_t vs2 = vdupq_n_u32(0);
        uint32x4_t vs1_0 = vdupq_n_u32(s1);

        // Process up to NMAX bytes before reduction
        size_t block = MIN(len, NMAX);
        size_t nblocks = block / 16;

        for (size_t i = 0; i < nblocks; i++) {
            uint8x16_t vbuf = vld1q_u8(buf);

            // s1 += sum(bytes)
            uint16x8_t sum16 = vpaddlq_u8(vbuf);
            uint32x4_t sum32 = vpaddlq_u16(sum16);
            vs1 = vaddq_u32(vs1, sum32);

            // s2 += 16 * s1_prev + weighted_sum(bytes)
            vs2 = vshlq_n_u32(vs1_0, 4);  // 16 * s1
            // Multiply-accumulate: weighted position sum
            uint16x8_t prod = vmull_u8(vget_low_u8(vbuf), vget_low_u8(vtaps));
            prod = vmlal_u8(prod, vget_high_u8(vbuf), vget_high_u8(vtaps));
            vs2 = vaddq_u32(vs2, vpaddlq_u16(prod));

            vs1_0 = vs1;
            buf += 16;
        }

        // Horizontal reduction
        s1 += vaddvq_u32(vs1);
        s2 += vaddvq_u32(vs2);
        s1 %= BASE;
        s2 %= BASE;
        len -= nblocks * 16;
    }
    return s1 | (s2 << 16);
}
```

Key NEON intrinsics used:
- `vpaddlq_u8` — Pairwise add long (u8→u16)
- `vpaddlq_u16` — Pairwise add long (u16→u32)
- `vmull_u8` — Multiply long (u8×u8→u16)
- `vmlal_u8` — Multiply-accumulate long
- `vaddvq_u32` — Horizontal sum across vector (AArch64)

---

## Hardware CRC-32 (`crc32_acle.c`)

Uses ARMv8 CRC32 instructions via ACLE (ARM C Language Extensions):

```c
Z_INTERNAL uint32_t crc32_acle(uint32_t crc, const uint8_t *buf, size_t len) {
    crc = ~crc;  // CRC32 instructions use inverted convention

    // Process 8 bytes at a time
    while (len >= 8) {
        crc = __crc32d(crc, *(uint64_t *)buf);
        buf += 8;
        len -= 8;
    }

    // Process 4 bytes
    if (len >= 4) {
        crc = __crc32w(crc, *(uint32_t *)buf);
        buf += 4;
        len -= 4;
    }

    // Process remaining bytes
    while (len--) {
        crc = __crc32b(crc, *buf++);
    }

    return ~crc;
}
```

The `__crc32b`, `__crc32w`, `__crc32d` intrinsics compile to single CRC32
instructions, computing CRC-32 of 1/4/8 bytes per instruction.

---

## PMULL CRC-32 (`crc32_pmull.c`)

For larger data, polynomial multiply (PMULL) provides higher throughput
via carry-less multiplication, similar to x86 PCLMULQDQ:

```c
Z_INTERNAL uint32_t crc32_pmull(uint32_t crc, const uint8_t *buf, size_t len) {
    poly128_t fold_const;
    uint64x2_t crc0, crc1, crc2, crc3;

    // Initialize four accumulators with first 64 bytes
    crc0 = veorq_u64(vld1q_u64((uint64_t *)buf),
                      vcombine_u64(vcreate_u64(crc), vcreate_u64(0)));
    // ... crc1, crc2, crc3

    // Main fold loop: 64 bytes per iteration
    while (len >= 64) {
        // vmull_p64: 64×64→128-bit polynomial multiply
        poly128_t h0 = vmull_p64(vgetq_lane_u64(crc0, 0), fold_lo);
        poly128_t h1 = vmull_p64(vgetq_lane_u64(crc0, 1), fold_hi);
        crc0 = veorq_u64(vreinterpretq_u64_p128(h0),
                          vreinterpretq_u64_p128(h1));
        crc0 = veorq_u64(crc0, vld1q_u64((uint64_t *)buf));
        // repeat for crc1..crc3
    }

    // Barrett reduction to 32-bit CRC
}
```

With `has_eor3` (SHA3 extension), three-way XOR is done in a single
instruction:

```c
#ifdef ARM_FEATURE_SHA3
    // EOR3: a ^= b ^ c in one instruction
    crc0 = vreinterpretq_u64_u8(veor3q_u8(
        vreinterpretq_u8_p128(h0),
        vreinterpretq_u8_p128(h1),
        vreinterpretq_u8_u64(data)));
#endif
```

---

## NEON String Comparison (`compare256_neon.c`)

```c
Z_INTERNAL uint32_t compare256_neon(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;
    do {
        uint8x16_t v0 = vld1q_u8(src0 + len);
        uint8x16_t v1 = vld1q_u8(src1 + len);
        uint8x16_t cmp = vceqq_u8(v0, v1);

        // Check if all bytes matched
        uint64_t mask_lo = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 0);
        uint64_t mask_hi = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 1);

        if (mask_lo != ~0ULL) {
            // First mismatch in lower 8 bytes
            return len + (__builtin_ctzll(~mask_lo) >> 3);
        }
        if (mask_hi != ~0ULL) {
            return len + 8 + (__builtin_ctzll(~mask_hi) >> 3);
        }
        len += 16;
    } while (len < 256);
    return 256;
}
```

---

## NEON Slide Hash (`slide_hash_neon.c`)

```c
Z_INTERNAL void slide_hash_neon(deflate_state *s) {
    unsigned n;
    Pos *p;
    uint16x8_t vw = vdupq_n_u16((uint16_t)s->w_size);

    n = HASH_SIZE;
    p = &s->head[n];
    do {
        p -= 8;
        uint16x8_t val = vld1q_u16(p);
        val = vqsubq_u16(val, vw);   // Saturating subtract
        vst1q_u16(p, val);
        n -= 8;
    } while (n);

    // Same loop for s->prev[0..w_size-1]
    n = s->w_size;
    p = &s->prev[n];
    do {
        p -= 8;
        uint16x8_t val = vld1q_u16(p);
        val = vqsubq_u16(val, vw);
        vst1q_u16(p, val);
        n -= 8;
    } while (n);
}
```

`vqsubq_u16` performs unsigned saturating subtract — values below zero
clamp to zero rather than wrapping.

---

## NEON Chunk Memory Set (`chunkset_neon.c`)

Used during inflate for back-reference copies:

```c
Z_INTERNAL uint8_t* chunkmemset_safe_neon(uint8_t *out, uint8_t *from,
                                           unsigned dist, unsigned len) {
    if (dist == 1) {
        // Broadcast single byte
        uint8x16_t vfill = vdupq_n_u8(*from);
        while (len >= 16) {
            vst1q_u8(out, vfill);
            out += 16;
            len -= 16;
        }
    } else if (dist == 2) {
        uint8x16_t v = vreinterpretq_u8_u16(vdupq_n_u16(*(uint16_t *)from));
        // ...
    } else if (dist >= 16) {
        // Standard copy
        while (len >= 16) {
            vst1q_u8(out, vld1q_u8(from));
            out += 16;
            from += 16;
            len -= 16;
        }
    } else {
        // Replicate dist-byte pattern into 16 bytes
        // ...
    }
    return out;
}
```

---

## CRC-Based Hash Insertion (`insert_string_acle.c`)

When ARMv8 CRC32 instructions are available, they provide excellent hash
distribution:

```c
Z_INTERNAL Pos insert_string_acle(deflate_state *s, Pos str, unsigned count) {
    Pos idx;
    for (unsigned i = 0; i < count; i++) {
        uint32_t val = *(uint32_t *)(s->window + str + i);
        uint32_t h = __crc32w(0, val);
        h &= s->hash_mask;
        idx = s->head[h];
        s->prev[(str + i) & s->w_mask] = idx;
        s->head[h] = (Pos)(str + i);
    }
    return idx;
}
```

---

## CMake Configuration

ARM features are detected via compiler intrinsic checks:

```cmake
option(WITH_NEON "Build with NEON SIMD" ON)
option(WITH_ACLE "Build with ACLE CRC" ON)

# AArch64 compiler flags
if(WITH_NEON)
    check_c_compiler_flag("-march=armv8-a+simd" HAS_NEON)
    if(HAS_NEON)
        set_property(SOURCE arch/arm/adler32_neon.c APPEND
                     PROPERTY COMPILE_OPTIONS -march=armv8-a+simd)
        # ... other NEON sources
        add_definitions(-DARM_NEON)
    endif()
endif()

if(WITH_ACLE)
    check_c_compiler_flag("-march=armv8-a+crc" HAS_CRC32)
    if(HAS_CRC32)
        set_property(SOURCE arch/arm/crc32_acle.c APPEND
                     PROPERTY COMPILE_OPTIONS -march=armv8-a+crc)
        add_definitions(-DARM_ACLE_CRC_HASH)
    endif()
    check_c_compiler_flag("-march=armv8-a+crypto" HAS_PMULL)
    if(HAS_PMULL)
        set_property(SOURCE arch/arm/crc32_pmull.c APPEND
                     PROPERTY COMPILE_OPTIONS -march=armv8-a+crypto)
        add_definitions(-DARM_PMULL_CRC)
    endif()
endif()
```

---

## Performance Notes

| Operation | NEON | CRC32 HW | PMULL |
|---|---|---|---|
| Adler-32 | ~8 bytes/cycle | — | — |
| CRC-32 | — | ~4 bytes/cycle | ~16 bytes/cycle |
| CRC-32+Copy | — | — | ~12 bytes/cycle |
| Compare256 | ~16 bytes/cycle | — | — |
| Slide Hash | ~8 entries/cycle | — | — |

Apple Silicon (M1+) provides particularly fast CRC32 and PMULL
implementations with low latency per instruction.

On Cortex-A55 and similar in-order cores, the throughput numbers are roughly
halved compared to Cortex-A76/A78 and Apple Silicon out-of-order cores.
