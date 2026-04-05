# Gzip Support

## Overview

Neozip provides a complete gzip file I/O layer on top of the core
deflate/inflate engine. This layer is implemented in three files:

- `gzlib.c` — Shared state management, file open/close, seeking
- `gzread.c` — Gzip file reading (decompression)
- `gzwrite.c` — Gzip file writing (compression)
- `gzguts.h` — Internal structures and constants

The gzip API is enabled by the `WITH_GZFILEOP` CMake option (ON by default).

---

## The `gz_state` Structure

From `gzguts.h`:

```c
typedef struct {
    // Identification
    PREFIX3(stream) strm;   // Inflate/deflate stream
    int mode;               // GZ_READ or GZ_WRITE
    int fd;                 // File descriptor
    char *path;             // Path for error messages
    unsigned size;          // Buffer size (default GZBUFSIZE)

    // Buffering
    unsigned want;          // Requested buffer size
    unsigned char *in;      // Input buffer (read mode)
    unsigned char *out;     // Output buffer
    int direct;             // 0=compressed, 1=passthrough (not gzip)

    // Position tracking
    z_off64_t start;        // Start of compressed data (after header)
    z_off64_t raw;          // Raw (compressed) file position
    z_off64_t pos;          // Uncompressed data position
    int eof;                // End of input file reached
    int past;               // Read past end of input

    // Error tracking
    int err;                // Error code
    char *msg;              // Error message (or NULL)
    int how;                // 0=output, 1=copy, 2=decompress

    // Write mode
    int level;              // Compression level
    int strategy;           // Compression strategy
    int reset;              // true if deflateReset needed

    // Seeking
    z_off64_t skip;         // Bytes to skip during next read

    // Peek
    int seek;               // Seek request pending
} gz_state;
```

### Constants

```c
#define GZBUFSIZE   131072   // Default buffer size (128 KB)
#define GZ_READ     7247     // Sentinel for read mode
#define GZ_WRITE    31153    // Sentinel for write mode
#define GZ_APPEND   1       // Mode flag for append
```

The sentinel values `GZ_READ` and `GZ_WRITE` are non-obvious integers
chosen to catch state corruption.

---

## File Open (`gzlib.c`)

### `gzopen()` / `gzdopen()`

```c
gzFile PREFIX(gzopen)(const char *path, const char *mode);
gzFile PREFIX(gzdopen)(int fd, const char *mode);
gzFile PREFIX(gzopen64)(const char *path, const char *mode);
```

The mode string supports:
- `r` — Read (decompress)
- `w` — Write (compress)
- `a` — Append (compress, append to existing file)
- `0-9` — Compression level
- `f` — `Z_FILTERED` strategy
- `h` — `Z_HUFFMAN_ONLY` strategy
- `R` — `Z_RLE` strategy
- `F` — `Z_FIXED` strategy
- `T` — Direct/transparent (no compression)

### `gz_state_init()`

```c
static void gz_state_init(gz_state *state) {
    state->size = 0;
    state->want = GZBUFSIZE;
    state->in = NULL;
    state->out = NULL;
    state->direct = 0;
    state->err = Z_OK;
    state->pos = 0;
    state->strm.avail_in = 0;
}
```

### `gz_buffer_alloc()`

Allocates I/O buffers:

```c
static int gz_buffer_alloc(gz_state *state) {
    unsigned size = state->want;

    if (state->mode == GZ_READ) {
        // Read: input buffer = size, output buffer = size * 2
        state->in = malloc(size);
        state->out = malloc(size << 1);
        state->size = size;
    } else {
        // Write: output buffer = size
        state->in = NULL;
        state->out = malloc(size);
        state->size = size;
    }
    return 0;
}
```

In read mode, the output buffer is doubled to handle cases where
decompression expands data significantly within a single call.

---

## Reading (`gzread.c`)

### Read Pipeline

```
gz_read() → gz_fetch() → gz_decomp() → inflate()
                       ↘ gz_look() (header detection)
```

### `gz_look()` — Header Detection

Determines if the file is gzip-compressed or raw:

```c
static int gz_look(gz_state *state) {
    // Read enough to check for gzip magic number
    if (state->strm.avail_in < 2) {
        // Read from file
        int got = read(state->fd, state->in, state->size);
        state->strm.avail_in = got;
        state->strm.next_in = state->in;
    }

    // Check for gzip magic (1f 8b)
    if (state->strm.avail_in >= 2 &&
        state->in[0] == 0x1f && state->in[1] == 0x8b) {
        // Initialize inflate for gzip
        inflateInit2(&state->strm, 15 + 16);  // windowBits + 16 = gzip
        state->how = 2;   // Decompress mode
    } else {
        // Not gzip — pass through directly
        state->direct = 1;
        state->how = 1;   // Copy mode
    }
}
```

### `gz_decomp()` — Decompression

```c
static int gz_decomp(gz_state *state) {
    int ret;
    unsigned had = state->strm.avail_out;

    // Call inflate
    ret = PREFIX(inflate)(&state->strm, Z_NO_FLUSH);
    state->pos += had - state->strm.avail_out;

    if (ret == Z_STREAM_END) {
        // End of gzip member — may be concatenated gzip
        inflateReset(&state->strm);
        state->how = 0;  // Need to look for next member
    }
    return 0;
}
```

### `gz_fetch()` — Fetch More Data

```c
static int gz_fetch(gz_state *state) {
    do {
        switch (state->how) {
        case 0:  // Look for gzip header
            if (gz_look(state) == -1) return -1;
            if (state->how == 0) return 0;  // EOF
            break;
        case 1:  // Copy raw data
            if (gz_load(state, state->out, state->size << 1, &got) == -1)
                return -1;
            state->pos += got;
            break;
        case 2:  // Decompress
            if (state->strm.avail_in == 0) {
                // Refill input buffer
                int got = read(state->fd, state->in, state->size);
                state->strm.avail_in = got;
                state->strm.next_in = state->in;
            }
            if (gz_decomp(state) == -1) return -1;
            break;
        }
    } while (state->strm.avail_out && !state->eof);
    return 0;
}
```

### Public Read API

```c
int PREFIX(gzread)(gzFile file, void *buf, unsigned len);
int PREFIX(gzgetc)(gzFile file);                // Read single character
char *PREFIX(gzgets)(gzFile file, char *buf, int len);  // Read line
z_off_t PREFIX(gzungetc)(int c, gzFile file);           // Push back character
int PREFIX(gzdirect)(gzFile file);                       // Check if raw
```

---

## Writing (`gzwrite.c`)

### Write Pipeline

```
gz_write() → gz_comp() → deflate()
```

### `gz_write_init()` — Lazy Initialisation

```c
static int gz_write_init(gz_state *state) {
    // Allocate output buffer
    gz_buffer_alloc(state);

    // Initialize deflate
    state->strm.next_out = state->out;
    state->strm.avail_out = state->size;

    int ret = PREFIX(deflateInit2)(&state->strm,
        state->level, Z_DEFLATED,
        15 + 16,  // windowBits + 16 = gzip wrapping
        DEF_MEM_LEVEL, state->strategy);

    return ret == Z_OK ? 0 : -1;
}
```

### `gz_comp()` — Compress Buffered Data

```c
static int gz_comp(gz_state *state, int flush) {
    int ret;
    unsigned have;

    // Deflate until done
    do {
        if (state->strm.avail_out == 0) {
            // Flush output buffer to file
            have = state->size;
            if (write(state->fd, state->out, have) != have) {
                state->err = Z_ERRNO;
                return -1;
            }
            state->strm.next_out = state->out;
            state->strm.avail_out = state->size;
        }
        ret = PREFIX(deflate)(&state->strm, flush);
    } while (ret == Z_OK && state->strm.avail_out == 0);

    if (flush == Z_FINISH && ret == Z_STREAM_END) {
        // Write final output
        have = state->size - state->strm.avail_out;
        if (have && write(state->fd, state->out, have) != have) {
            state->err = Z_ERRNO;
            return -1;
        }
    }
    return 0;
}
```

### Public Write API

```c
int PREFIX(gzwrite)(gzFile file, const void *buf, unsigned len);
int PREFIX(gzputc)(gzFile file, int c);
int PREFIX(gzputs)(gzFile file, const char *s);
int PREFIX(gzprintf)(gzFile file, const char *format, ...);
int PREFIX(gzflush)(gzFile file, int flush);
int PREFIX(gzsetparams)(gzFile file, int level, int strategy);
```

---

## Seeking and Position

```c
z_off64_t PREFIX(gzseek64)(gzFile file, z_off64_t offset, int whence);
z_off64_t PREFIX(gztell64)(gzFile file);
z_off64_t PREFIX(gzoffset64)(gzFile file);
int PREFIX(gzrewind)(gzFile file);
int PREFIX(gzeof)(gzFile file);
```

### Forward Seeking

For read mode, seeking forward decompresses and discards data:

```c
// In gzseek: forward seek in read mode
state->skip = offset;  // Will be consumed during next gz_fetch
```

### Backward Seeking

Backward seeking requires a full rewind and re-decompression:

```c
// Must reset and decompress from the beginning
gzrewind(file);
state->skip = offset;
```

---

## Gzip Format

A gzip file (RFC 1952) consists of:

```
┌──────────────────────────────────┐
│ Header (10+ bytes)               │
│   1F 8B  — magic number          │
│   08     — compression method    │
│   FLG    — flags                 │
│   MTIME  — modification time     │
│   XFL    — extra flags           │
│   OS     — operating system      │
│   [EXTRA] [NAME] [COMMENT] [HCRC]│
├──────────────────────────────────┤
│ Compressed data (deflate)        │
├──────────────────────────────────┤
│ Trailer (8 bytes)                │
│   CRC32  — CRC of original data  │
│   ISIZE  — size of original data │
└──────────────────────────────────┘
```

FLG bits:
- `FTEXT` (0x01) — Text mode hint
- `FHCRC` (0x02) — Header CRC present
- `FEXTRA` (0x04) — Extra field present
- `FNAME` (0x08) — Original filename present
- `FCOMMENT` (0x10) — Comment present

### Concatenated Gzip

Multiple gzip members can be concatenated. `gzread()` transparently
decompresses all members in sequence, resetting the inflate state at
each `Z_STREAM_END` boundary.

---

## Error Handling

```c
int PREFIX(gzerror)(gzFile file, int *errnum);   // Get error message
void PREFIX(gzclearerr)(gzFile file);            // Clear error state
```

The `gz_state.err` field tracks errors:
- `Z_OK` — No error
- `Z_ERRNO` — System I/O error (check `errno`)
- `Z_STREAM_ERROR` — Invalid state
- `Z_DATA_ERROR` — Corrupted gzip data
- `Z_MEM_ERROR` — Memory allocation failure
- `Z_BUF_ERROR` — Insufficient buffer space

---

## Close

```c
int PREFIX(gzclose)(gzFile file);
int PREFIX(gzclose_r)(gzFile file);   // Close read-mode file
int PREFIX(gzclose_w)(gzFile file);   // Close write-mode file
```

`gzclose_w()` flushes pending output with `Z_FINISH`, writes the
remaining compressed data, then calls `deflateEnd()`.

`gzclose_r()` calls `inflateEnd()` and frees buffers.

Both close the file descriptor (unless opened via `gzdopen()` with
the `F` flag to leave the fd open).
