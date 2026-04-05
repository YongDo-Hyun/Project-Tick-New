# Zlib Integration

## Overview

libnbt++ provides optional zlib support for reading and writing gzip/zlib-compressed NBT data, which is the standard format for Minecraft world files (`level.dat`, region files, etc.).

The zlib integration is in the `zlib` namespace and operates through standard C++ stream wrappers. It is conditionally compiled via the `NBT_USE_ZLIB` CMake option (default: `ON`).

Defined in:
- `include/io/zlib_streambuf.h` — Base streambuf with z_stream management
- `include/io/izlibstream.h` / `src/io/izlibstream.cpp` — Decompression stream
- `include/io/ozlibstream.h` / `src/io/ozlibstream.cpp` — Compression stream

---

## Build Configuration

```cmake
option(NBT_USE_ZLIB "Build with zlib stream support" ON)
```

When enabled, CMake finds zlib and links against it:
```cmake
if(NBT_USE_ZLIB)
    find_package(ZLIB REQUIRED)
    target_link_libraries(${NBT_NAME} PRIVATE ZLIB::ZLIB)
    target_compile_definitions(${NBT_NAME} PUBLIC NBT_HAVE_ZLIB)
endif()
```

The `NBT_HAVE_ZLIB` preprocessor macro is defined publicly, allowing downstream code to conditionally use zlib features:

```cpp
#ifdef NBT_HAVE_ZLIB
#include <io/izlibstream.h>
#include <io/ozlibstream.h>
#endif
```

---

## zlib_streambuf — Base Class

```cpp
class zlib_streambuf : public std::streambuf
{
protected:
    z_stream zstr;
    std::vector<char> in;
    std::vector<char> out;

    static const size_t bufsize = 32768; // 32 KB
};
```

This abstract base provides the shared z_stream state and I/O buffers used by both inflate and deflate streambufs.

- `zstr`: The zlib `z_stream` struct controlling compression/decompression state
- `in`: Input buffer (32 KB)
- `out`: Output buffer (32 KB)
- `bufsize`: Buffer size constant (32768 bytes)

---

## zlib_error — Exception Class

```cpp
class zlib_error : public std::runtime_error
{
public:
    zlib_error(const z_stream& zstr, int status);
    int status() const { return status_; }

private:
    int status_;
};
```

Thrown on zlib API failures. Wraps the error message from `zstr.msg` (if available) along with the numeric error code.

---

## Decompression: izlibstream

### inflate_streambuf

```cpp
class inflate_streambuf : public zlib_streambuf
{
public:
    explicit inflate_streambuf(std::istream& input,
                               int window_bits = 32 + MAX_WBITS);
    ~inflate_streambuf();

    int_type underflow() override;

private:
    std::istream& is;
    bool stream_end = false;
};
```

**Constructor:**
```cpp
inflate_streambuf::inflate_streambuf(std::istream& input, int window_bits)
    : is(input)
{
    in.resize(bufsize);
    out.resize(bufsize);

    zstr.zalloc = Z_NULL;
    zstr.zfree = Z_NULL;
    zstr.opaque = Z_NULL;
    zstr.avail_in = 0;
    zstr.next_in = Z_NULL;

    int status = inflateInit2(&zstr, window_bits);
    if (status != Z_OK)
        throw zlib_error(zstr, status);
}
```

The default `window_bits = 32 + MAX_WBITS` (typically `32 + 15 = 47`) enables **automatic format detection** — zlib will detect whether the data is raw deflate, zlib, or gzip format. This is critical because Minecraft NBT files may use either gzip or zlib compression.

**underflow() — Buffered decompression:**

```cpp
inflate_streambuf::int_type inflate_streambuf::underflow()
{
    if (stream_end)
        return traits_type::eof();

    zstr.next_out = reinterpret_cast<Bytef*>(out.data());
    zstr.avail_out = out.size();

    while (zstr.avail_out == out.size()) {
        if (zstr.avail_in == 0) {
            is.read(in.data(), in.size());
            auto bytes_read = is.gcount();
            if (bytes_read == 0) {
                setg(nullptr, nullptr, nullptr);
                return traits_type::eof();
            }
            zstr.next_in = reinterpret_cast<Bytef*>(in.data());
            zstr.avail_in = bytes_read;
        }

        int status = inflate(&zstr, Z_NO_FLUSH);
        if (status == Z_STREAM_END) {
            // Seek back unused input so the underlying stream
            // position is correct for any subsequent reads
            if (zstr.avail_in > 0)
                is.seekg(-static_cast<int>(zstr.avail_in),
                         std::ios::cur);
            stream_end = true;
            break;
        }
        if (status != Z_OK)
            throw zlib_error(zstr, status);
    }

    auto decompressed = out.size() - zstr.avail_out;
    if (decompressed == 0)
        return traits_type::eof();

    setg(out.data(), out.data(), out.data() + decompressed);
    return traits_type::to_int_type(out[0]);
}
```

Key behaviors:
- Reads compressed data in 32 KB chunks from the underlying stream
- Decompresses into the output buffer
- On `Z_STREAM_END`, seeks the underlying stream back by the number of unconsumed bytes, so subsequent reads on the same stream work correctly (important for concatenated data)
- Throws `zlib_error` on decompression errors

### izlibstream

```cpp
class izlibstream : public std::istream
{
public:
    explicit izlibstream(std::istream& input,
                         int window_bits = 32 + MAX_WBITS);

private:
    inflate_streambuf buf;
};
```

A simple `std::istream` wrapper around `inflate_streambuf`:

```cpp
izlibstream::izlibstream(std::istream& input, int window_bits)
    : std::istream(&buf), buf(input, window_bits)
{}
```

Usage:
```cpp
std::ifstream file("level.dat", std::ios::binary);
zlib::izlibstream zs(file);
auto result = nbt::io::read_compound(zs);
```

---

## Compression: ozlibstream

### deflate_streambuf

```cpp
class deflate_streambuf : public zlib_streambuf
{
public:
    explicit deflate_streambuf(std::ostream& output,
                               int level = Z_DEFAULT_COMPRESSION,
                               int window_bits = MAX_WBITS);
    ~deflate_streambuf();

    int_type overflow(int_type ch) override;
    int sync() override;
    void close();

private:
    std::ostream& os;
    bool closed_ = false;

    void deflate_chunk(int flush);
};
```

**Constructor:**
```cpp
deflate_streambuf::deflate_streambuf(std::ostream& output,
                                     int level, int window_bits)
    : os(output)
{
    in.resize(bufsize);
    out.resize(bufsize);

    zstr.zalloc = Z_NULL;
    zstr.zfree = Z_NULL;
    zstr.opaque = Z_NULL;

    int status = deflateInit2(&zstr, level, Z_DEFLATED,
                              window_bits, 8, Z_DEFAULT_STRATEGY);
    if (status != Z_OK)
        throw zlib_error(zstr, status);

    setp(in.data(), in.data() + in.size());
}
```

Parameters:
- `level`: Compression level (0–9, or `Z_DEFAULT_COMPRESSION` = -1)
- `window_bits`: Format control
  - `MAX_WBITS` (15): raw zlib format
  - `MAX_WBITS + 16` (31): gzip format

**overflow() — Buffer full, deflate and flush:**
```cpp
deflate_streambuf::int_type deflate_streambuf::overflow(int_type ch)
{
    deflate_chunk(Z_NO_FLUSH);
    if (ch != traits_type::eof()) {
        *pptr() = traits_type::to_char_type(ch);
        pbump(1);
    }
    return ch;
}
```

**sync() — Flush current buffer:**
```cpp
int deflate_streambuf::sync()
{
    deflate_chunk(Z_SYNC_FLUSH);
    return 0;
}
```

**deflate_chunk() — Core compression loop:**
```cpp
void deflate_streambuf::deflate_chunk(int flush)
{
    zstr.next_in = reinterpret_cast<Bytef*>(pbase());
    zstr.avail_in = pptr() - pbase();

    do {
        zstr.next_out = reinterpret_cast<Bytef*>(out.data());
        zstr.avail_out = out.size();

        int status = deflate(&zstr, flush);
        if (status != Z_OK && status != Z_STREAM_END
            && status != Z_BUF_ERROR)
            throw zlib_error(zstr, status);

        auto compressed = out.size() - zstr.avail_out;
        if (compressed > 0)
            os.write(out.data(), compressed);
    } while (zstr.avail_out == 0);

    setp(in.data(), in.data() + in.size());
}
```

**close() — Finalize compression:**
```cpp
void deflate_streambuf::close()
{
    if (closed_) return;
    closed_ = true;
    deflate_chunk(Z_FINISH);
}
```

Must be called to write the final compressed block with `Z_FINISH`.

### ozlibstream

```cpp
class ozlibstream : public std::ostream
{
public:
    explicit ozlibstream(std::ostream& output,
                         int level = Z_DEFAULT_COMPRESSION,
                         bool gzip = true);

    void close();

private:
    deflate_streambuf buf;
};
```

**Constructor:**
```cpp
ozlibstream::ozlibstream(std::ostream& output, int level, bool gzip)
    : std::ostream(&buf),
      buf(output, level, MAX_WBITS + (gzip ? 16 : 0))
{}
```

The `gzip` parameter (default: `true`) controls the output format:
- `true`: gzip format (`window_bits = MAX_WBITS + 16 = 31`)
- `false`: raw zlib format (`window_bits = MAX_WBITS = 15`)

**close() — Exception-safe stream finalization:**
```cpp
void ozlibstream::close()
{
    try {
        buf.close();
    } catch (...) {
        setstate(std::ios::badbit);
        if (exceptions() & std::ios::badbit)
            throw;
    }
}
```

`close()` catches exceptions from `buf.close()` and converts them to a badbit state. If the stream has badbit exceptions enabled, it re-throws.

---

## Format Detection

### Automatic Detection (Reading)

The default inflate `window_bits = 32 + MAX_WBITS` enables automatic format detection:

| Bits | Format |
|------|--------|
| `MAX_WBITS` (15) | Raw deflate |
| `MAX_WBITS + 16` (31) | Gzip only |
| `MAX_WBITS + 32` (47) | Auto-detect gzip or zlib |

The library defaults to auto-detect (47), so it handles both formats transparently.

### Explicit Format (Writing)

When writing, you must choose:

```cpp
// Gzip format (default for Minecraft)
zlib::ozlibstream gzip_out(file);                            // gzip=true
zlib::ozlibstream gzip_out(file, Z_DEFAULT_COMPRESSION, true); // explicit

// Zlib format
zlib::ozlibstream zlib_out(file, Z_DEFAULT_COMPRESSION, false);
```

---

## Usage Examples

### Reading a Gzip-Compressed NBT File

```cpp
std::ifstream file("level.dat", std::ios::binary);
zlib::izlibstream zs(file);
auto [name, root] = nbt::io::read_compound(zs);
```

### Writing a Gzip-Compressed NBT File

```cpp
std::ofstream file("level.dat", std::ios::binary);
zlib::ozlibstream zs(file);  // gzip by default
nbt::io::write_tag("", root, zs);
zs.close();  // MUST call close() to finalize
```

### Roundtrip Compression

```cpp
// Compress
std::stringstream ss;
{
    zlib::ozlibstream zs(ss);
    nbt::io::write_tag("test", compound, zs);
    zs.close();
}

// Decompress
ss.seekg(0);
{
    zlib::izlibstream zs(ss);
    auto [name, tag] = nbt::io::read_compound(zs);
    // tag now contains the original compound
}
```

### Controlling Compression Level

```cpp
// No compression (fastest)
zlib::ozlibstream fast(file, Z_NO_COMPRESSION);

// Best compression (slowest)
zlib::ozlibstream best(file, Z_BEST_COMPRESSION);

// Default compression (balanced)
zlib::ozlibstream default_level(file, Z_DEFAULT_COMPRESSION);

// Specific level (0-9)
zlib::ozlibstream level6(file, 6);
```

---

## Error Handling

### zlib_error

All zlib API failures throw `zlib::zlib_error`:

```cpp
try {
    zlib::izlibstream zs(file);
    auto result = nbt::io::read_compound(zs);
} catch (const zlib::zlib_error& e) {
    std::cerr << "Zlib error: " << e.what()
              << " (status: " << e.status() << ")\n";
}
```

Common error codes:
| Status | Meaning |
|--------|---------|
| `Z_DATA_ERROR` | Corrupted compressed data |
| `Z_MEM_ERROR` | Insufficient memory |
| `Z_BUF_ERROR` | Buffer/progress error |
| `Z_STREAM_ERROR` | Invalid parameters |

### Stream State

After decompression errors, the `izlibstream` may be in a bad state. After `ozlibstream::close()` catches an exception from the deflate buffer, it sets `std::ios::badbit` on the stream.

---

## Resource Management

### Destructors

Both `inflate_streambuf` and `deflate_streambuf` call the corresponding zlib cleanup in their destructors:

```cpp
inflate_streambuf::~inflate_streambuf()
{
    inflateEnd(&zstr);
}

deflate_streambuf::~deflate_streambuf()
{
    deflateEnd(&zstr);
}
```

These release all memory allocated by zlib. The destructors are noexcept-safe — `inflateEnd`/`deflateEnd` do not throw.

### Important: Call close() Before Destruction

For `ozlibstream`, you **must** call `close()` before the stream is destroyed to ensure the final compressed block is flushed. The destructor calls `deflateEnd()` but does **not** call `close()` — failing to close will produce a truncated compressed file.

```cpp
{
    std::ofstream file("output.dat", std::ios::binary);
    zlib::ozlibstream zs(file);
    nbt::io::write_tag("", root, zs);
    zs.close();  // Required!
}  // file and zs destroyed safely
```
