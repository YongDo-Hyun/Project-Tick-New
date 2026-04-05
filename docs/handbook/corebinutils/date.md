# date — Display and Set System Date

## Overview

`date` displays the current date and time, or sets the system clock. It
supports strftime-based format strings, ISO 8601 output, RFC 2822 output,
timezone overrides, date arithmetic via "vary" adjustments, and input
date parsing.

**Source**: `date/date.c`, `date/vary.c`, `date/vary.h`
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause / BSD-2-Clause (vary.c)

## Synopsis

```
date [-jnRu] [-r seconds | filename] [-I[date|hours|minutes|seconds|ns]]
     [-f input_fmt] [-v [+|-]val[ymwdHMS]] [-z output_zone]
     [+output_format] [[[[[cc]yy]mm]dd]HH]MM[.ss]]
```

## Options

| Flag | Description |
|------|-------------|
| `-j` | Do not try to set the system clock |
| `-n` | Same as `-j` (compatibility) |
| `-R` | RFC 2822 format output |
| `-u` | Use UTC instead of local time |
| `-r seconds` | Display time from epoch seconds |
| `-r filename` | Display modification time of file |
| `-I[precision]` | ISO 8601 format output |
| `-f input_fmt` | Parse input date using strptime format |
| `-v adjustment` | Adjust date components (can be repeated) |
| `-z timezone` | Use specified timezone for output |

## Source Analysis

### date.c — Main Implementation

#### Key Data Structures

```c
struct iso8601_fmt {
    const char *refname;        /* "date", "hours", "minutes", etc. */
    const char *format_string;  /* strftime format */
    bool include_zone;          /* Whether to append timezone */
};

struct strbuf {
    char *data;
    size_t len;
    size_t cap;
};

struct options {
    const char *input_format;       /* -f format string */
    const char *output_zone;        /* -z timezone */
    const char *reference_arg;      /* -r argument */
    const char *time_operand;       /* MMDDhhmm... or parsed date */
    const char *format_operand;     /* +format string */
    struct vary *vary_chain;        /* -v adjustments */
    const struct iso8601_fmt *iso8601_selected;
    bool no_set;                    /* -j flag */
    bool rfc2822;                   /* -R flag */
    bool use_utc;                   /* -u flag */
};
```

#### ISO 8601 Formats

```c
static const struct iso8601_fmt iso8601_fmts[] = {
    { "date",    "%Y-%m-%d",              false },
    { "hours",   "%Y-%m-%dT%H",           true  },
    { "minutes", "%Y-%m-%dT%H:%M",        true  },
    { "seconds", "%Y-%m-%dT%H:%M:%S",     true  },
    { "ns",      "%Y-%m-%dT%H:%M:%S,%N",  true  },
};
```

#### Key Functions

| Function | Purpose |
|----------|---------|
| `main()` | Parse options, resolve time, format output |
| `parse_args()` | Option-by-option argument processing |
| `validate_options()` | Check for conflicting options |
| `set_timezone_or_die()` | Apply timezone via `setenv("TZ", ...)` |
| `read_reference_time()` | Get time from `-r` argument (epoch or file mtime) |
| `read_current_time()` | Get current time via `clock_gettime(2)` |
| `parse_legacy_time()` | Parse `[[[[cc]yy]mm]dd]HH]MM[.ss]` format |
| `parse_formatted_time()` | Parse via `strptime(3)` with `-f` format |
| `parse_time_operand()` | Dispatch to legacy or formatted parser |
| `set_system_time()` | Set clock via `clock_settime(2)` |
| `apply_variations()` | Apply `-v` adjustments to broken-down time |
| `expand_format_string()` | Expand `%N` (nanoseconds) in format strings |
| `render_format()` | Format via `strftime(3)` with extensions |
| `render_iso8601()` | Generate ISO 8601 output with timezone |
| `render_numeric_timezone()` | Format `+HHMM` timezone offset |
| `print_line_and_exit()` | Write output and exit |

#### Main Flow

```c
int main(int argc, char **argv)
{
    parse_args(argc, argv, &options);
    validate_options(&options);
    setlocale(LC_TIME, "");

    if (options.use_utc)
        set_timezone_or_die("UTC0", "TZ=UTC0");

    if (options.reference_arg != NULL)
        read_reference_time(options.reference_arg, &ts);
    else
        read_current_time(&ts, &resolution);

    if (options.time_operand != NULL) {
        parse_time_operand(&options, &ts, &ts);
        if (!options.no_set)
            set_system_time(&ts);
    }

    localtime_or_die(ts.tv_sec, &tm);
    apply_variations(&options, &tm);

    /* Render output based on -I, -R, or +format */
    output = render_format(format, &tm, ts.tv_nsec, resolution.tv_nsec);
    print_line_and_exit(output);
}
```

#### String Buffer Implementation

`date.c` includes a custom growable string buffer for format expansion:

```c
static void strbuf_init(struct strbuf *buf);
static void strbuf_reserve(struct strbuf *buf, size_t extra);
static void strbuf_append_mem(struct strbuf *buf, const char *data, size_t len);
static void strbuf_append_char(struct strbuf *buf, char ch);
static void strbuf_append_str(struct strbuf *buf, const char *text);
static char *strbuf_finish(struct strbuf *buf);
```

#### Nanosecond Format Extension

The `%N` format specifier (not in standard `strftime`) is expanded
manually before passing to `strftime(3)`:

```c
static void
append_nsec_digits(struct strbuf *buf, const char *pending, size_t len,
    long nsec, long resolution)
{
    /* Format nanoseconds with appropriate precision based on resolution */
}
```

### vary.c — Date Arithmetic

The `-v` flag enables relative date adjustments. Multiple `-v` flags can
be chained to build complex date expressions.

#### Adjustment Types

| Code | Unit | Example |
|------|------|---------|
| `y` | Years | `-v +1y` (next year) |
| `m` | Months | `-v -3m` (3 months ago) |
| `w` | Weeks | `-v +2w` (2 weeks forward) |
| `d` | Days | `-v +1d` (tomorrow) |
| `H` | Hours | `-v -6H` (6 hours ago) |
| `M` | Minutes | `-v +30M` (30 minutes forward) |
| `S` | Seconds | `-v -10S` (10 seconds ago) |

#### Named Values

Month names and weekday names can be used with `=`:

```sh
date -v =monday     # Next Monday
date -v =january    # Set month to January
```

#### Implementation

```c
struct trans {
    int64_t value;
    const char *name;
};

static const struct trans trans_mon[] = {
    { 1, "january" }, { 2, "february" }, { 3, "march" },
    { 4, "april" },   { 5, "may" },      { 6, "june" },
    { 7, "july" },    { 8, "august" },    { 9, "september" },
    { 10, "october" },{ 11, "november" }, { 12, "december" },
    { -1, NULL }
};

static const struct trans trans_wday[] = {
    { 0, "sunday" },   { 1, "monday" },  { 2, "tuesday" },
    { 3, "wednesday" },{ 4, "thursday" },{ 5, "friday" },
    { 6, "saturday" }, { -1, NULL }
};
```

The `vary_apply()` function processes each adjustment in the chain,
calling specific adjuster functions:

```c
static int adjyear(struct tm *tm, char type, int64_t value, bool normalize);
static int adjmon(struct tm *tm, char type, int64_t value, bool is_text, bool normalize);
static int adjday(struct tm *tm, char type, int64_t value, bool normalize);
static int adjwday(struct tm *tm, char type, int64_t value, bool is_text, bool normalize);
static int adjhour(struct tm *tm, char type, int64_t value, bool normalize);
static int adjmin(struct tm *tm, char type, int64_t value, bool normalize);
static int adjsec(struct tm *tm, char type, int64_t value, bool normalize);
```

Each adjuster modifies the broken-down `struct tm` and calls
`normalize_tm()` to fix rolled-over fields via `mktime(3)`.

### Timezone Handling

```c
static void
set_timezone_or_die(const char *tz_value, const char *what)
{
    if (setenv("TZ", tz_value, 1) != 0)
        die_errno("setenv %s", what);
    tzset();
}
```

The `-u` flag sets `TZ=UTC0`. The `-z` flag sets `TZ` to the specified
value only for output formatting (input parsing uses the original timezone).

### Legacy Time Format

The BSD legacy format `[[[[cc]yy]mm]dd]HH]MM[.ss]` is parsed
right-to-left:

```c
static void
parse_legacy_time(const char *text, const struct timespec *base, struct timespec *ts)
{
    /* Parse from rightmost position:
     * 1. [.ss]  - optional seconds
     * 2. MM     - minutes (required)
     * 3. HH     - hours
     * 4. dd     - day
     * 5. mm     - month
     * 6. [cc]yy - year
     */
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `clock_gettime(2)` | Read current time with nanosecond precision |
| `clock_settime(2)` | Set system clock (requires root) |
| `stat(2)` | Get file modification time for `-r filename` |
| `setenv(3)` | Set `TZ` environment variable |
| `strftime(3)` | Format broken-down time |
| `strptime(3)` | Parse time from formatted string |
| `mktime(3)` | Normalize broken-down time |
| `localtime_r(3)` | Thread-safe time conversion |

## Format Strings

`date` supports all `strftime(3)` format specifiers plus:

| Specifier | Meaning |
|-----------|---------|
| `%N` | Nanoseconds (extension, expanded before strftime) |
| `%+` | Default format (equivalent to `%a %b %e %T %Z %Y`) |

Common `strftime` specifiers:

| Specifier | Output |
|-----------|--------|
| `%Y` | 4-digit year (2026) |
| `%m` | Month (01-12) |
| `%d` | Day (01-31) |
| `%H` | Hour (00-23) |
| `%M` | Minute (00-59) |
| `%S` | Second (00-60) |
| `%T` | Time as `%H:%M:%S` |
| `%Z` | Timezone abbreviation |
| `%z` | Numeric timezone (`+0000`) |
| `%s` | Epoch seconds |
| `%a` | Abbreviated weekday |
| `%b` | Abbreviated month |

## Examples

```sh
# Default output
date
# → Sat Apr  5 14:30:00 UTC 2026

# Custom format
date "+%Y-%m-%d %H:%M:%S"
# → 2026-04-05 14:30:00

# ISO 8601
date -Iseconds
# → 2026-04-05T14:30:00+00:00

# RFC 2822
date -R
# → Sat, 05 Apr 2026 14:30:00 +0000

# UTC
date -u

# Epoch seconds
date +%s
# → 1775578200

# Date arithmetic: tomorrow
date -v +1d

# Date arithmetic: last Monday
date -v -monday

# Date arithmetic: 3 months from now, at midnight
date -v +3m -v 0H -v 0M -v 0S

# Parse input format
date -f "%Y%m%d" "20260405" "+%A, %B %d"
# → Sunday, April 05

# Display file modification time
date -r /etc/passwd

# Display epoch time
date -r 1775578200
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Error (invalid format, failed to set time, etc.) |
