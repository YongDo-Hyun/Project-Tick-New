# hostname — Get or Set the System Hostname

## Overview

`hostname` reads or sets the system hostname. On Linux it uses `uname(2)` to
read and `sethostname(2)` to write. The `-f` (FQDN) option is explicitly
unsupported because resolving a fully qualified domain name requires
NSS/DNS, which is outside the scope of a core utility.

**Source**: `hostname/hostname.c` (single file)
**Origin**: BSD 4.4, University of California, Berkeley
**License**: BSD-3-Clause

## Synopsis

```
hostname [-s | -d] [name-of-host]
```

## Options

| Flag | Description |
|------|-------------|
| `-s` | Print short hostname (truncate at first `.`) |
| `-d` | Print domain part only (after first `.`) |
| `-f` | **Not supported on Linux** — exits with error |

## Source Analysis

### Data Structures

```c
struct options {
    bool short_name;    /* -s: truncate at first dot */
    bool domain_only;   /* -d: print after first dot */
    bool set_mode;      /* hostname was provided as argument */
    const char *new_hostname;
};
```

### Functions

| Function | Purpose |
|----------|---------|
| `main()` | Dispatch between get/set modes |
| `parse_args()` | `getopt(3)` option parsing |
| `dup_hostname()` | Fetch hostname from `uname(2)` and duplicate it |
| `print_hostname()` | Print full, short, or domain part |
| `set_hostname()` | Set hostname via `sethostname(2)` |
| `linux_hostname_max()` | Query max hostname length from UTS namespace |

### Reading the Hostname

```c
static char *
dup_hostname(void)
{
    struct utsname uts;

    if (uname(&uts) < 0)
        err(1, "uname");
    return strdup(uts.nodename);
}
```

### Setting the Hostname

```c
static void
set_hostname(const char *name)
{
    size_t max_len = linux_hostname_max();
    size_t len = strlen(name);

    if (len > max_len)
        errx(1, "hostname too long: %zu > %zu", len, max_len);

    if (sethostname(name, len) < 0)
        err(1, "sethostname");
}
```

### Short/Domain Modes

```c
static void
print_hostname(const char *hostname, const struct options *opts)
{
    if (opts->short_name) {
        /* Truncate at first '.' */
        const char *dot = strchr(hostname, '.');
        if (dot)
            printf("%.*s\n", (int)(dot - hostname), hostname);
        else
            puts(hostname);
    } else if (opts->domain_only) {
        /* Print after first '.' or empty */
        const char *dot = strchr(hostname, '.');
        puts(dot ? dot + 1 : "");
    } else {
        puts(hostname);
    }
}
```

### Max Hostname Length

```c
static size_t
linux_hostname_max(void)
{
    long val = sysconf(_SC_HOST_NAME_MAX);
    return (val > 0) ? (size_t)val : 64;
}
```

## System Calls Used

| Syscall | Purpose |
|---------|---------|
| `uname(2)` | Read current hostname |
| `sethostname(2)` | Set new hostname (requires `CAP_SYS_ADMIN`) |
| `sysconf(3)` | Query `_SC_HOST_NAME_MAX` |

## Examples

```sh
# Print hostname
hostname

# Print short hostname
hostname -s

# Print domain part
hostname -d

# Set hostname (requires root)
hostname myserver.example.com
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Error (sethostname failed, invalid option) |

## Differences from GNU hostname

- No `-f` / `--fqdn` — Linux requires NSS for FQDN resolution
- No `--ip-address` / `-i`
- No `--alias` / `-a`
- No `--all-fqdns` / `--all-ip-addresses`
- Simpler: read or set only, no DNS lookups
