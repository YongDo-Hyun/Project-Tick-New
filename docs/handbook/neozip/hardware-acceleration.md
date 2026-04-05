# Hardware Acceleration

## Overview

Neozip dispatches compression and decompression operations to the best
available hardware-accelerated implementation at runtime. This is achieved
through a function table (`functable`), CPU feature detection, and
architecture-specific source files compiled with appropriate SIMD flags.

---

## CPU Feature Detection

### `cpu_features.c`

The entry point for feature detection:

```c
void Z_INTERNAL cpu_check_features(struct cpu_features *features) {
    // Zero out features
    memset(features, 0, sizeof(*features));

#if defined(X86_FEATURES)
    x86_check_features(features);
#elif defined(ARM_FEATURES)
    arm_check_features(features);
#elif defined(POWER_FEATURES)
    power_check_features(features);
#elif defined(S390_FEATURES)
    s390_check_features(features);
#elif defined(RISCV_FEATURES)
    riscv_check_features(features);
#elif defined(LOONGARCH_FEATURES)
    loongarch_check_features(features);
#endif
}
```

### CPU Feature Structures

```c
struct cpu_features {
    union {
#if defined(X86_FEATURES)
        struct x86_cpu_features x86;
#elif defined(ARM_FEATURES)
        struct arm_cpu_features arm;
#elif defined(POWER_FEATURES)
        struct power_cpu_features power;
#elif defined(S390_FEATURES)
        struct s390_cpu_features s390;
#elif defined(RISCV_FEATURES)
        struct riscv_cpu_features riscv;
#elif defined(LOONGARCH_FEATURES)
        struct loongarch_cpu_features loongarch;
#endif
    };
};
```

Each architecture defines its own feature structure:

**x86** (`x86_features.h`):
```c
struct x86_cpu_features {
    int has_avx2;
    int has_avx512f;
    int has_avx512dq;
    int has_avx512bw;
    int has_avx512vl;
    int has_avx512_common;   // All of f+dq+bw+vl
    int has_avx512vnni;
    int has_sse2;
    int has_ssse3;
    int has_sse41;
    int has_sse42;
    int has_pclmulqdq;
    int has_vpclmulqdq;
    int has_os_save_ymm;
    int has_os_save_zmm;
};
```

**ARM** (`arm_features.h`):
```c
struct arm_cpu_features {
    int has_simd;        // ARMv6 SIMD
    int has_neon;        // ARMv7+ NEON / AArch64 ASIMD
    int has_crc32;       // CRC32 instructions
    int has_pmull;       // PMULL (polynomial multiply long)
    int has_eor3;        // SHA3 EOR3 instruction
    int has_fast_pmull;  // High-perf PMULL available
};
```

---

## x86 Feature Detection

`x86_check_features()` in `arch/x86/x86_features.c` uses CPUID:

```c
void Z_INTERNAL x86_check_features(struct cpu_features *features) {
    unsigned eax, ebx, ecx, edx;

    // CPUID leaf 1
    cpuid(1, &eax, &ebx, &ecx, &edx);
    features->x86.has_sse2 = !!(edx & (1 << 26));
    features->x86.has_ssse3 = !!(ecx & (1 << 9));
    features->x86.has_sse41 = !!(ecx & (1 << 19));
    features->x86.has_sse42 = !!(ecx & (1 << 20));
    features->x86.has_pclmulqdq = !!(ecx & (1 << 1));

    // Check XSAVE support for YMM
    if (ecx & (1 << 27)) {  // OSXSAVE
        uint64_t xcr0 = xgetbv(0);
        features->x86.has_os_save_ymm = (xcr0 & 0x06) == 0x06;
        features->x86.has_os_save_zmm = (xcr0 & 0xe6) == 0xe6;
    }

    // CPUID leaf 7
    cpuidp(7, 0, &eax, &ebx, &ecx, &edx);
    if (features->x86.has_os_save_ymm) {
        features->x86.has_avx2 = !!(ebx & (1 << 5));
    }
    if (features->x86.has_os_save_zmm) {
        features->x86.has_avx512f  = !!(ebx & (1 << 16));
        features->x86.has_avx512dq = !!(ebx & (1 << 17));
        features->x86.has_avx512bw = !!(ebx & (1 << 30));
        features->x86.has_avx512vl = !!(ebx & (1 << 31));
        features->x86.has_vpclmulqdq = !!(ecx & (1 << 10));
        features->x86.has_avx512vnni = !!(ecx & (1 << 11));
    }
    features->x86.has_avx512_common =
        features->x86.has_avx512f && features->x86.has_avx512dq &&
        features->x86.has_avx512bw && features->x86.has_avx512vl;
}
```

### OS Support Verification

YMM (256-bit) and ZMM (512-bit) registers require OS support to save/restore
context during context switches. `xgetbv(0)` reads the XCR0 register:

- Bits 1+2 set → YMM state is saved (required for AVX2)
- Bits 1+2+5+6+7 set → ZMM state is saved (required for AVX-512)

Without OS support, using AVX2/AVX-512 instructions will fault.

---

## ARM Feature Detection

ARM detection in `arch/arm/arm_features.c` uses platform-specific methods:

**Linux**: Reads `/proc/cpuinfo` or uses `getauxval(AT_HWCAP)`:
```c
features->arm.has_neon = !!(hwcap & HWCAP_NEON);     // AArch32
features->arm.has_neon = !!(hwcap & HWCAP_ASIMD);    // AArch64
features->arm.has_crc32 = !!(hwcap & HWCAP_CRC32);
features->arm.has_pmull = !!(hwcap & HWCAP_PMULL);
```

**macOS**: Uses `sysctlbyname()`:
```c
features->arm.has_neon = 1;  // Always available on Apple Silicon
features->arm.has_crc32 = has_feature("hw.optional.armv8_crc32");
features->arm.has_pmull = has_feature("hw.optional.arm.FEAT_PMULL");
```

---

## The Function Table

### `functable_s` Structure

```c
struct functable_s {
    adler32_func          adler32;
    adler32_copy_func     adler32_copy;
    compare256_func       compare256;
    crc32_func            crc32;
    crc32_copy_func       crc32_copy;
    inflate_fast_func     inflate_fast;
    longest_match_func    longest_match;
    longest_match_slow_func longest_match_slow;
    slide_hash_func       slide_hash;
    chunksize_func        chunksize;
    chunkmemset_safe_func chunkmemset_safe;
};
```

### Dispatch Cascade

`functable.c` initialises the function table using a cascade:

```c
static void init_functable(void) {
    struct cpu_features cf;
    cpu_check_features(&cf);

    // Start with generic C implementations
    functable.adler32        = adler32_c;
    functable.crc32          = crc32_braid;
    functable.compare256     = compare256_c;
    functable.longest_match  = longest_match_c;
    functable.slide_hash     = slide_hash_c;
    functable.inflate_fast   = inflate_fast_c;
    functable.chunksize      = chunksize_c;
    functable.chunkmemset_safe = chunkmemset_safe_c;

#ifdef X86_SSE2
    if (cf.x86.has_sse2) {
        functable.chunksize = chunksize_sse2;
        functable.chunkmemset_safe = chunkmemset_safe_sse2;
        functable.compare256 = compare256_sse2;
        functable.inflate_fast = inflate_fast_sse2;
        functable.longest_match = longest_match_sse2;
        functable.slide_hash = slide_hash_sse2;
    }
#endif
#ifdef X86_SSSE3
    if (cf.x86.has_ssse3) {
        functable.adler32 = adler32_ssse3;
    }
#endif
#ifdef X86_SSE42
    if (cf.x86.has_sse42) {
        functable.adler32 = adler32_sse42;
        functable.compare256 = compare256_sse42;
        functable.longest_match = longest_match_sse42;
    }
#endif
#ifdef X86_PCLMULQDQ
    if (cf.x86.has_pclmulqdq) {
        functable.crc32 = crc32_pclmulqdq;
    }
#endif
#ifdef X86_AVX2
    if (cf.x86.has_avx2) {
        functable.adler32 = adler32_avx2;
        functable.chunksize = chunksize_avx2;
        functable.chunkmemset_safe = chunkmemset_safe_avx2;
        functable.compare256 = compare256_avx2;
        functable.inflate_fast = inflate_fast_avx2;
        functable.longest_match = longest_match_avx2;
        functable.slide_hash = slide_hash_avx2;
    }
#endif
#ifdef X86_AVX512
    if (cf.x86.has_avx512_common) {
        functable.adler32 = adler32_avx512;
        functable.slide_hash = slide_hash_avx512;
    }
#endif
#ifdef X86_AVX512VNNI
    if (cf.x86.has_avx512vnni) {
        functable.adler32 = adler32_avx512_vnni;
    }
#endif
#ifdef X86_VPCLMULQDQ
    if (cf.x86.has_vpclmulqdq && cf.x86.has_avx512_common) {
        functable.crc32 = crc32_vpclmulqdq;
    }
#endif

    // ARM cascade
#ifdef ARM_NEON
    if (cf.arm.has_neon) {
        functable.adler32 = adler32_neon;
        functable.chunksize = chunksize_neon;
        functable.chunkmemset_safe = chunkmemset_safe_neon;
        functable.compare256 = compare256_neon;
        functable.slide_hash = slide_hash_neon;
        functable.inflate_fast = inflate_fast_neon;
        functable.longest_match = longest_match_neon;
    }
#endif
#ifdef ARM_ACLE_CRC_HASH
    if (cf.arm.has_crc32) {
        functable.crc32 = crc32_acle;
    }
#endif

    // Store with release semantics for thread safety
    atomic_store_explicit(&functable_init_done, 1, memory_order_release);
}
```

Later features override earlier ones, so the best available implementation
wins.

### Thread-Safe Initialisation

The function table uses atomic operations for thread safety:

```c
static atomic_int functable_init_done = 0;
static struct functable_s functable;

#define FUNCTABLE_CALL(name) \
    do { \
        if (!atomic_load_explicit(&functable_init_done, memory_order_acquire)) \
            init_functable(); \
    } while (0); \
    functable.name
```

The first call triggers initialisation; subsequent calls skip it via the
atomic flag.

---

## Accelerated Operations

### 1. Adler-32 Checksum

**Scalar**: `adler32_c()` — byte-by-byte with NMAX blocking
**SIMD**: Uses horizontal sum and dot product — SSE4.1/SSSE3/AVX2/AVX-512/VNNI/NEON/VMX/Power8/RVV/LASX

### 2. CRC-32 Checksum

**Scalar**: `crc32_braid()` — braided 5-word parallel CRC
**SIMD**: Carry-less multiplication (CLMUL) for fast polynomial arithmetic — PCLMULQDQ/VPCLMULQDQ/PMULL/Power8/Zbc

### 3. String Matching (`compare256`)

Compares up to 256 bytes to find the longest match:

**Scalar**: `compare256_c()` — byte-by-byte comparison
**SIMD**: Loads 16/32/64 bytes at a time, uses `_mm_cmpeq_epi8` + `_mm_movemask_epi8` (SSE2) or equivalent to find the first mismatch

### 4. Longest Match

Wraps `compare256` with hash chain walking:

```c
longest_match_func longest_match;
longest_match_slow_func longest_match_slow;
```

The `_slow` variant also inserts intermediate hash entries for level ≥ 9.

### 5. Slide Hash

Slides the hash table down by one window's worth:

**Scalar**: `slide_hash_c()` — loop over HASH_SIZE + w_size entries
**SIMD**: Processes 8/16/32 entries at a time using saturating subtract

```c
// SSE2 example pattern:
__m128i vw = _mm_set1_epi16((uint16_t)s->w_size);
for (...) {
    __m128i v = _mm_loadu_si128(p);
    v = _mm_subs_epu16(v, vw);  // Saturating subtract
    _mm_storeu_si128(p, v);
}
```

### 6. Chunk Memory Set (`chunkmemset_safe`)

Fast memset/memcpy for inflate back-reference copying:

**Scalar**: `chunkmemset_safe_c()` — handles overlap via small loops
**SIMD**: Replicates the pattern into vector registers, handles even
overlapping copies via broadcast

### 7. Inflate Fast

The hot inner loop of the inflate engine:

**Scalar**: `inflate_fast_c()` — standard decode loop
**SIMD**: Uses wider copy operations from chunkmemset for the back-reference
copy step

---

## Compile-Time vs Runtime Detection

### Runtime Detection (Default)

Enabled by `WITH_RUNTIME_CPU_DETECTION=ON` (default):
- All SIMD variants are compiled as separate translation units
- `functable.c` selects the best at runtime
- Binary runs on any CPU of the target architecture

### Native Compilation

Enabled by `WITH_NATIVE_INSTRUCTIONS=ON`:
- Compiles with `-march=native` (or equivalent)
- The compiler uses host CPU features directly
- Slightly faster: no function pointer indirection
- Binary only runs on the build machine's CPU (or compatible)

### Disabling Runtime Detection

`DISABLE_RUNTIME_CPU_DETECTION` can be defined to skip runtime checks
and use only the generic C implementations, useful for constrained
environments.

---

## Adding a New Architecture

To add SIMD support for a new architecture:

1. **Create `arch/<arch>/` directory** with feature detection and implementations
2. **Define a feature structure** in `<arch>_features.h`
3. **Implement `<arch>_check_features()`** using platform-specific detection
4. **Implement accelerated functions** matching the `functable_s` signatures
5. **Add dispatch entries** in `functable.c` guarded by feature flags
6. **Add CMake detection** in `CMakeLists.txt`:
   ```cmake
   check_<arch>_intrinsics()
   if(WITH_<ARCH>_<FEATURE>)
       add_compile_options(-m<flag>)
       list(APPEND ZLIB_ARCH_SRCS arch/<arch>/...)
       add_definitions(-D<ARCH>_<FEATURE>)
   endif()
   ```

---

## Supported Architecture Matrix

| Architecture | adler32 | crc32 | compare256 | longest_match | slide_hash | inflate_fast | chunkmemset |
|---|---|---|---|---|---|---|---|
| x86 SSE2 | – | – | ✓ | ✓ | ✓ | ✓ | ✓ |
| x86 SSSE3 | ✓ | – | – | – | – | – | – |
| x86 SSE4.1 | ✓ | – | – | – | – | – | – |
| x86 SSE4.2 | ✓ | – | ✓ | ✓ | – | – | – |
| x86 PCLMULQDQ | – | ✓ | – | – | – | – | – |
| x86 AVX2 | ✓ | – | ✓ | ✓ | ✓ | ✓ | ✓ |
| x86 AVX-512 | ✓ | – | – | – | ✓ | – | – |
| x86 AVX-512+VNNI | ✓ | – | – | – | – | – | – |
| x86 VPCLMULQDQ | – | ✓ | – | – | – | – | – |
| ARM NEON | ✓ | – | ✓ | ✓ | ✓ | ✓ | ✓ |
| ARM CRC32 | – | ✓ | – | – | – | – | – |
| ARM PMULL | – | ✓ | – | – | – | – | – |
| Power VMX | ✓ | – | – | – | ✓ | – | – |
| Power8 | ✓ | ✓ | – | – | – | – | – |
| Power9 | – | – | ✓ | ✓ | – | – | – |
| RISC-V RVV | ✓ | – | ✓ | ✓ | – | – | – |
| s390 CRC | – | ✓ | – | – | – | – | – |
| LoongArch LSX | ✓ | – | – | – | ✓ | – | ✓ |
| LoongArch LASX | ✓ | – | – | – | – | – | – |
