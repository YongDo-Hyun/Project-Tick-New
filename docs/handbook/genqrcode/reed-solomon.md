# genqrcode / libqrencode — Reed-Solomon Internals

## Overview

The `rsecc.c` module implements a systematic Reed-Solomon encoder over GF(2^8). It is the sole error-correction engine used by libqrencode for both full QR Code and Micro QR Code. The module provides a single public function, `RSECC_encode()`, and manages all GF arithmetic, generator polynomials, and thread safety internally.

---

## Galois Field GF(2^8)

### Primitive Polynomial

```c
static int proot = 0x11d;  // x^8 + x^4 + x^3 + x^2 + 1
```

This is the standard QR Code primitive polynomial (identical to the one used in AES). In binary: `100011101`. It is irreducible over GF(2) and generates a field of 256 elements.

### Log and Antilog Tables

Two 256-entry lookup tables enable O(1) multiplication in GF(2^8):

```c
static unsigned char alpha[256];   // antilog: alpha[i] = α^i
static unsigned char aindex[256];  // log: aindex[α^i] = i
```

**Initialization** (`RSECC_init()`):

```c
static void RSECC_init(void)
{
    int i, b;

    alpha[0] = 1;          // α^0 = 1
    aindex[0] = 0;         // log(0) = undefined, set to 0
    aindex[1] = 0;         // log(1) = 0

    for(i = 1; i < 255; i++) {
        b = alpha[i-1] << 1;       // multiply by x
        if(b & 0x100) {
            b ^= proot;            // reduce mod primitive
        }
        alpha[i] = (unsigned char)b;
        aindex[b] = i;
    }
}
```

After initialization:
- `alpha[0] = 1, alpha[1] = 2, alpha[2] = 4, ..., alpha[7] = 128, alpha[8] = 29, ...`
- `alpha[255]` is not computed (wraps to `alpha[0]`)
- `aindex[0] = 0` is a sentinel (log of 0 is undefined in GF)

**GF multiplication**: To multiply `a * b`:
```
result = alpha[(aindex[a] + aindex[b]) % 255]
```
Special case: if either operand is 0, result is 0.

---

## Generator Polynomials

### Theory

For `t` ECC codewords, the generator polynomial is:

$$g(x) = (x + \alpha^0)(x + \alpha^1) \cdots (x + \alpha^{t-1})$$

This produces a degree-`t` polynomial. The encoder divides the data polynomial by `g(x)` and the remainder becomes the ECC codewords.

### Cache

Generator polynomials are cached to avoid recomputation:

```c
static int generator_initialized[29] = {0};   // flags for el=2..30
static unsigned char generator[29][31];         // polynomial coefficients
```

The array is indexed by `ecc_length - 2`, supporting ECC lengths 2 through 30. QR Code never needs more than 30 ECC codewords per block.

### Construction

`generator_init()` builds the polynomial incrementally:

```c
static void generator_init(int el)
{
    int i, j;
    unsigned char *g = generator[el - 2];

    g[0] = 1;   // start with g(x) = 1

    for(i = 0; i < el; i++) {
        // multiply g(x) by (x + α^i)
        g[i+1] = 1;
        for(j = i; j > 0; j--) {
            if(g[j] != 0) {
                g[j] = g[j-1] ^ alpha[(aindex[g[j]] + i) % 255];
            } else {
                g[j] = g[j-1];
            }
        }
        g[0] = alpha[(aindex[g[0]] + i) % 255];
    }
}
```

**Walk-through for `el = 2`** (2 ECC codewords):

1. Start: g = [1]
2. Multiply by (x + α^0) = (x + 1): g = [1, 1] → coefficients of x + 1
3. Multiply by (x + α^1) = (x + 2): g = [2, 3, 1] → coefficients of 2x^2 + 3x + 1

Wait — actually the coefficients are stored in reverse order (constant term first):
- `g[0]` = constant term
- `g[el]` = leading coefficient (always 1)

**Walk-through for `el = 4`** (4 ECC codewords):
1. g(x) = 1
2. g(x) = (x + 1) → g(x) = x + 1
3. g(x) *= (x + α) → g(x) = x^2 + 3x + 2
4. g(x) *= (x + α^2) → g(x) = x^3 + (some)x^2 + (some)x + (some)
5. g(x) *= (x + α^3) → degree-4 polynomial

---

## The Encode Function

```c
int RSECC_encode(int data_length, int ecc_length,
                 const unsigned char *data, unsigned char *ecc)
```

**Parameters:**
- `data_length` — Number of data codewords
- `ecc_length` — Number of ECC codewords to generate (2–30)
- `data` — Input data codewords
- `ecc` — Output buffer for ECC codewords (must be pre-allocated, `ecc_length` bytes)

**Returns:** 0 on success, -1 on failure.

### Thread Safety

```c
#ifdef HAVE_LIBPTHREAD
static pthread_mutex_t RSECC_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

int RSECC_encode(int data_length, int ecc_length,
                 const unsigned char *data, unsigned char *ecc)
{
    int i, j;
    unsigned char feedback;
    unsigned char *gen;

#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock(&RSECC_mutex);
#endif

    if(!generator_initialized[ecc_length - 2]) {
        if(!initialized) {
            RSECC_init();
            initialized = 1;
        }
        generator_init(ecc_length);
        generator_initialized[ecc_length - 2] = 1;
    }

#ifdef HAVE_LIBPTHREAD
    pthread_mutex_unlock(&RSECC_mutex);
#endif
```

The mutex protects **only the initialization** path. Once tables are initialized, subsequent calls proceed without locking.

### Encoding Algorithm

```c
    gen = generator[ecc_length - 2];

    memset(ecc, 0, ecc_length);
    for(i = 0; i < data_length; i++) {
        feedback = aindex[data[i] ^ ecc[0]];
        if(feedback != 255) {
            for(j = 1; j < ecc_length; j++) {
                ecc[j] ^= alpha[(feedback + aindex[gen[ecc_length - j]]) % 255];
            }
        }
        memmove(&ecc[0], &ecc[1], ecc_length - 1);
        if(feedback != 255) {
            ecc[ecc_length - 1] = alpha[(feedback + aindex[gen[0]]) % 255];
        } else {
            ecc[ecc_length - 1] = 0;
        }
    }

    return 0;
}
```

This is a standard LFSR (linear feedback shift register) implementation of systematic RS encoding:

1. **For each data byte `data[i]`:**
   - XOR with current first ECC byte: `data[i] ^ ecc[0]`
   - Convert to log domain: `feedback = aindex[...]`
   - If non-zero (`!= 255` sentinel):
     - Multiply feedback by each generator coefficient and XOR into corresponding ECC position
   - Shift ECC register left by one (via `memmove`)
   - Set last ECC position to `α^(feedback + log(g[0]))` or 0

2. **After processing all data bytes**, `ecc[]` contains the RS remainder — the ECC codewords.

### Why `feedback != 255`?

The check `feedback != 255` is a zero-check. When `data[i] ^ ecc[0] == 0`, its log is undefined. The code uses 255 as a sentinel since `α^255 = α^0 = 1` (wraps around), but 0 has no valid logarithm. So when `aindex[0]` returns 0 (the initialized sentinel), the specific case where the XOR result is actually 0 must be caught.

Actually, looking more carefully: `aindex[0] = 0`, and `alpha[0] = 1`, so `aindex[0]` doesn't return 255. The check `feedback != 255` would only trigger if `data[i] ^ ecc[0]` maps to index 255 in the log table. Since `alpha[254]` is the last computed value and `alpha[255]` wraps, the value 255 in `aindex` isn't actually reached for any valid input. The zero case is when `data[i] ^ ecc[0] == 0`, giving `feedback = aindex[0] = 0`, which is non-zero and proceeds normally. The `!= 255` check is a safety guard for the edge case.

---

## How RS Blocks Are Created

In `qrencode.c`, `QRraw_new()` creates RS blocks:

```c
static QRRawCode *QRraw_new(QRinput *input) {
    QRRawCode *raw;
    int spec[5];

    QRspec_getEccSpec(input->version, input->level, spec);

    raw->dataLength = QRspec_getDataLength(input->version, input->level);
    raw->eccLength  = QRspec_getECCLength(input->version, input->level);
    raw->b1 = QRspec_rsBlockNum1(spec);
    raw->b2 = QRspec_rsBlockNum2(spec);
    raw->rsblock_num = raw->b1 + raw->b2;
    raw->rsblock = calloc(raw->rsblock_num, sizeof(RSblock));

    unsigned char *datacode = QRinput_getByteStream(input);
    unsigned char *ecccode  = malloc(raw->eccLength);

    int dl1 = QRspec_rsDataCodes1(spec);
    int el  = QRspec_rsEccCodes1(spec);  // same for both groups
    int dl2 = QRspec_rsDataCodes2(spec);

    unsigned char *dp = datacode;
    unsigned char *ep = ecccode;

    // Initialize group 1 blocks
    for(int i = 0; i < raw->b1; i++) {
        RSblock_initBlock(&raw->rsblock[i], dl1, dp, el, ep, RSECC_encode);
        dp += dl1;
        ep += el;
    }

    // Initialize group 2 blocks (if any)
    for(int i = 0; i < raw->b2; i++) {
        RSblock_initBlock(&raw->rsblock[raw->b1 + i], dl2, dp, el, ep, RSECC_encode);
        dp += dl2;
        ep += el;
    }

    return raw;
}
```

`RSblock_initBlock()` calls `RSECC_encode()` directly:

```c
static int RSblock_initBlock(RSblock *block, int dl, unsigned char *data,
                             int el, unsigned char *ecc,
                             RSECC_encoder encoder)
{
    block->dataLength = dl;
    block->data = data;
    block->eccLength = el;
    block->ecc = ecc;

    return encoder(dl, el, data, ecc);
}
```

There is no decoding — libqrencode only encodes. RS decoding is the scanner's responsibility.

---

## ECC Length Range

The generator cache supports ECC lengths 2–30:

```c
static int generator_initialized[29] = {0};  // indices 0..28 → el 2..30
static unsigned char generator[29][31];       // max degree 30, 31 coefficients
```

From the `eccTable` in `qrspec.c`, the maximum ECC per block is 30 codewords (used in version 40 at all EC levels). The minimum is 7 (version 1, level L).

For Micro QR, all versions use a single block with small ECC counts (3, 5, 6, 8, 10, or 14 depending on version/level).

---

## Performance Characteristics

- **Table initialization**: One-time cost. GF tables take ~510 bytes. Generator polynomials up to ~930 bytes total.
- **Encoding**: O(n × t) where n = data length, t = ECC length. The inner loop does 1 log lookup, 1 addition mod 255, 1 antilog lookup, and 1 XOR per generator coefficient.
- **Memory**: The `memmove()` in each iteration shifts `ecc_length - 1` bytes, which for typical blocks (el ≤ 30) is negligible.
- **Thread contention**: Minimal. The mutex is only held during initialization. After all generator polynomials are initialized (at most 29 different ones), encoding is lock-free.

---

## Mathematical Background

### Systematic Encoding

Given data polynomial $D(x) = d_{k-1}x^{k-1} + d_{k-2}x^{k-2} + \cdots + d_0$ and generator polynomial $g(x)$ of degree $t$:

1. Compute $x^t \cdot D(x)$ (shift data up by $t$ positions)
2. Divide: $x^t \cdot D(x) = q(x) \cdot g(x) + r(x)$
3. The codeword is $x^t \cdot D(x) - r(x) = x^t \cdot D(x) + r(x)$ (subtraction = addition in GF(2))

The remainder $r(x)$ has degree < $t$ and its coefficients are the ECC codewords.

### The LFSR Approach

The RSECC_encode implementation uses a shift-register approach equivalent to polynomial long division:

```
ecc = [0, 0, ..., 0]   (t zeros)

for each data byte d:
    feedback = log(d XOR ecc[0])
    shift ecc left by 1
    for j = 1 to t-1:
        ecc[j] ^= alpha[feedback + log(g[t-j])]
    ecc[t-1] = alpha[feedback + log(g[0])]
```

Each iteration processes one data codeword, maintaining the partial remainder in the `ecc` register. After all data is processed, the register contains the final remainder.

### Error Correction Capability

For `t` ECC codewords, the RS code can correct up to $\lfloor t/2 \rfloor$ symbol errors, or detect up to `t` symbol errors (or any combination where $2e + d \leq t$, with $e$ errors and $d$ erasures).

QR Code Level L with 7 ECC per block → 3 correctable symbol errors per block.
