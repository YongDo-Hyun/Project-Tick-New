# genqrcode / libqrencode — CLI Usage (`qrencode`)

## Overview

`qrencode` is the command-line tool for encoding data into QR Code symbols. It is built from `qrenc.c` when `WITH_TOOLS` is enabled (default: ON). The tool supports 14 output formats and all library features including Micro QR, structured append, and FNC1.

---

## Basic Usage

```bash
# Encode text to PNG
qrencode -o output.png "Hello, World!"

# Read from stdin
echo "Hello" | qrencode -o output.png

# Encode to terminal (UTF-8)
qrencode -t UTF8 "Hello"

# Encode to terminal (ANSI colors)
qrencode -t ANSI "Hello"
```

---

## Command-Line Options

### Input Options

| Flag | Long | Description |
|---|---|---|
| `-i FILE` | `--input=FILE` | Input file (default: stdin, `-` for stdin) |
| (positional) | | Input string (alternative to `-i` or stdin) |

### Output Options

| Flag | Long | Description |
|---|---|---|
| `-o FILE` | `--output=FILE` | Output file (default: stdout) |
| `-t TYPE` | `--type=TYPE` | Output format (see formats below) |
| `-s SIZE` | `--size=SIZE` | Module size in pixels (PNG/EPS/SVG/XPM) |
| `-m MARGIN` | `--margin=MARGIN` | Margin width in modules (default: 4 for QR, 2 for MQR) |
| `-d DPI` | `--dpi=DPI` | DPI for EPS output |
| `--rle` | | Run-length encoding for SVG |
| `--svg-path` | | Embed path in SVG |
| `--inline` | | Inline SVG (no XML header) |

### Encoding Options

| Flag | Long | Description |
|---|---|---|
| `-v VERSION` | `--symversion=VERSION` | Symbol version (0 = auto, 1–40 for QR, 1–4 for MQR) |
| `-l LEVEL` | `--level=LEVEL` | EC level: L, M, Q, H |
| `-8` | `--8bit` | 8-bit mode (no mode optimization) |
| `-k` | `--kanji` | Kanji mode (assume Shift-JIS input) |
| `-c` | `--casesensitive` | Case-sensitive encoding |
| `-S` | `--structured` | Structured append mode |
| `-M` | `--micro` | Micro QR Code |
| `--strict-version` | | Fail if data doesn't fit in specified version |

### Color Options

| Flag | Long | Description |
|---|---|---|
| `--foreground=RRGGBB[AA]` | | Foreground color (hex, default: 000000) |
| `--background=RRGGBB[AA]` | | Background color (hex, default: FFFFFF) |

### Other

| Flag | Long | Description |
|---|---|---|
| `-V` | `--version` | Print library version |
| `-h` | `--help` | Print help |

---

## Output Formats

The `--type` / `-t` flag accepts:

| Type | Description |
|---|---|
| `PNG` | PNG image (default when -o ends in .png) |
| `PNG32` | 32-bit PNG with alpha channel |
| `EPS` | Encapsulated PostScript |
| `SVG` | Scalable Vector Graphics |
| `XPM` | X PixMap |
| `ANSI` | ANSI terminal colors (2 rows per line) |
| `ANSI256` | ANSI 256-color terminal |
| `ASCII` | ASCII art (## for dark, spaces for light) |
| `ASCIIi` | Inverted ASCII art |
| `UTF8` | Unicode block characters (half-height modules) |
| `UTF8i` | Inverted UTF8 |
| `ANSIUTF8` | UTF-8 with ANSI color codes |
| `ANSIUTF8i` | Inverted ANSIUTF8 |
| `ANSI256UTF8` | UTF-8 with ANSI 256-color codes |

Auto-detection: if `-t` is not specified, the format is inferred from the output filename extension:
- `.png` → PNG
- `.eps` → EPS
- `.svg` → SVG
- `.xpm` → XPM
- Otherwise: PNG to file, UTF8 to terminal

Defined in `qrenc.c`:

```c
enum imageType {
    PNG_TYPE,
    PNG32_TYPE,
    EPS_TYPE,
    SVG_TYPE,
    XPM_TYPE,
    ANSI_TYPE,
    ANSI256_TYPE,
    ASCII_TYPE,
    ASCIIi_TYPE,
    UTF8_TYPE,
    UTF8i_TYPE,
    ANSIUTF8_TYPE,
    ANSIUTF8i_TYPE,
    ANSI256UTF8_TYPE
};
```

---

## Output Writers

### writePNG / writePNG32

```c
static int writePNG(const QRcode *qrcode, const char *outfile,
                    enum imageType type)
```

Uses libpng. Module size and margin affect dimensions. Supports foreground/background colors and alpha channel (PNG32).

### writeEPS

```c
static int writeEPS(const QRcode *qrcode, const char *outfile)
```

Generates PostScript with module rectangles. Respects DPI setting.

### writeSVG

```c
static int writeSVG(const QRcode *qrcode, const char *outfile)
```

Options:
- `--rle`: Uses run-length encoding for horizontal module runs (smaller files)
- `--svg-path`: Uses a single SVG path element instead of rectangles
- `--inline`: Omits XML declaration and DOCTYPE

### writeUTF8

```c
static void writeUTF8(const QRcode *qrcode, const char *outfile,
                       int use_ansi, int invert)
```

Uses Unicode block-drawing characters to display 2 rows per terminal line:
- `█` (U+2588): both dark
- `▀` (U+2580): top dark, bottom light
- `▄` (U+2584): top light, bottom dark
- ` `: both light

### writeANSI

```c
static void writeANSI(const QRcode *qrcode, const char *outfile,
                       int use256, int invert)
```

Uses ANSI escape codes for colored terminal output. Two rows per line with `▀` characters.

### writeASCII

```c
static void writeASCII(const QRcode *qrcode, const char *outfile, int invert)
```

Simple ASCII output: `##` for dark modules, `  ` for light modules.

---

## Encoding Logic

The `encode()` function dispatches to the appropriate library function:

```c
static QRcode *encode(const unsigned char *intext, int length)
{
    QRcode *code;

    if(micro) {
        if(eightbit) {
            code = QRcode_encodeDataMQR(length, intext, version, level);
        } else {
            code = QRcode_encodeStringMQR((char *)intext, version, level,
                                          hint, casesensitive);
        }
    } else if(eightbit) {
        code = QRcode_encodeData(length, intext, version, level);
    } else {
        code = QRcode_encodeString((char *)intext, version, level,
                                   hint, casesensitive);
    }

    return code;
}
```

### Structured Append

When `-S` is specified, `qrencodeStructured()` is called instead:

```c
static void qrencodeStructured(const unsigned char *intext, int length,
                                const char *outfile)
{
    QRcode_List *qrlist, *p;
    char filename[FILENAME_MAX];
    int i = 1;

    if(eightbit) {
        qrlist = QRcode_encodeDataStructured(length, intext, version, level);
    } else {
        qrlist = QRcode_encodeStringStructured((char *)intext, version, level,
                                                hint, casesensitive);
    }

    for(p = qrlist; p != NULL; p = p->next) {
        // Generate filename with sequence number
        snprintf(filename, FILENAME_MAX, "%s-%02d.png", outfile_base, i);
        writePNG(p->code, filename, image_type);
        i++;
    }
    QRcode_List_free(qrlist);
}
```

Each symbol in the structured set is written to a separate file with sequence numbering.

---

## Color Parsing

Foreground and background colors are parsed from hex strings:

```c
static int color_set(unsigned char color[4], const char *value)
{
    // Parse RRGGBB or RRGGBBAA hex string
    int len = strlen(value);
    if(len == 6) {
        sscanf(value, "%02x%02x%02x",
               (unsigned int *)&color[0],
               (unsigned int *)&color[1],
               (unsigned int *)&color[2]);
        color[3] = 255;  // fully opaque
    } else if(len == 8) {
        sscanf(value, "%02x%02x%02x%02x",
               (unsigned int *)&color[0],
               (unsigned int *)&color[1],
               (unsigned int *)&color[2],
               (unsigned int *)&color[3]);
    }
    // ...
}
```

Default colors:
```c
static unsigned char fg_color[4] = {0, 0, 0, 255};       // black
static unsigned char bg_color[4] = {255, 255, 255, 255};  // white
```

---

## Examples

### Generate PNG with custom version and EC level

```bash
qrencode -v 5 -l H -o code.png "Secure data"
```

### Generate SVG with custom colors

```bash
qrencode -t SVG --foreground=336699 --background=FFFFFF -o code.svg "Hello"
```

### Generate Micro QR to terminal

```bash
qrencode -M -v 3 -l L -t UTF8 "12345"
```

### Force 8-bit encoding (no mode optimization)

```bash
qrencode -8 -o code.png "Already know encoding mode"
```

### Structured append — split across multiple symbols

```bash
qrencode -S -v 5 -l M -o codes "Very long text that needs splitting..."
# Outputs: codes-01.png, codes-02.png, ...
```

### Case-insensitive (maximize alphanumeric mode)

```bash
qrencode -o code.png "HELLO WORLD"  # default: case-sensitive
qrencode -o code.png "hello world"  # -c not set: converted to uppercase
```

### Custom module size and margin

```bash
qrencode -s 10 -m 2 -o code.png "Hello"
# 10px per module, 2-module margin
```

### Inline SVG for HTML embedding

```bash
qrencode -t SVG --inline --svg-path -o- "Hello" > page.html
```

### Read binary data from file

```bash
qrencode -8 -i binary_file.dat -o code.png
```

### Print to terminal with ANSI colors

```bash
qrencode -t ANSI256 "https://example.com"
```

---

## Global Variables

Key globals in `qrenc.c` that control behavior:

```c
static int casesensitive = 1;
static int eightbit = 0;
static int version = 0;
static int size = 3;
static int margin = -1;
static int dpi = 72;
static int structured = 0;
static int rle = 0;
static int svg_path = 0;
static int micro = 0;
static int inline_svg = 0;
static int strict_versioning = 0;
static QRecLevel level = QR_ECLEVEL_L;
static QRencodeMode hint = QR_MODE_8;
static unsigned char fg_color[4] = {0, 0, 0, 255};
static unsigned char bg_color[4] = {255, 255, 255, 255};
```

---

## Dependencies

- **libpng** (optional): Required for PNG output. Disabled with `--without-png` / `-DWITHOUT_PNG=ON`.
- **zlib**: Required by libpng.

When libpng is not available, PNG output types are disabled and the tool falls back to text-based formats. The build system detects libpng via `pkg-config` or `FindPNG.cmake`.
