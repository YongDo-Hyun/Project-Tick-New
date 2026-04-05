# cgit — Caching System

## Overview

cgit implements a file-based output cache that stores the fully rendered
HTML/binary response for each unique request.  The cache avoids regenerating
pages for repeated identical requests.  When caching is disabled
(`cache-size=0`, the default), all output is written directly to `stdout`.

Source files: `cache.c`, `cache.h`.

## Cache Slot Structure

Each cached response is represented by a `cache_slot`:

```c
struct cache_slot {
    const char *key;       /* request identifier (URL-based) */
    int keylen;            /* strlen(key) */
    int ttl;               /* time-to-live in minutes */
    cache_fill_fn fn;      /* callback to regenerate content */
    int cache_fd;          /* fd for the cache file */
    int lock_fd;           /* fd for the .lock file */
    const char *cache_name;/* path: cache_root/hash(key) */
    const char *lock_name; /* path: cache_name + ".lock" */
    int match;             /* 1 if cache file matches key */
    struct stat cache_st;  /* stat of the cache file */
    int bufsize;           /* size of the header buffer */
    char buf[1024 + 4 * 20]; /* header: key + timestamps */
};
```

The `cache_fill_fn` typedef:

```c
typedef void (*cache_fill_fn)(void *cbdata);
```

This callback is invoked to produce the page content when the cache needs
filling.  The callback writes directly to `stdout`, which is redirected to the
lock file while cache filling is in progress.

## Hash Function

Cache file names are derived from the request key using the FNV-1 hash:

```c
unsigned long hash_str(const char *str)
{
    unsigned long h = 0x811c9dc5;
    unsigned char *s = (unsigned char *)str;
    while (*s) {
        h *= 0x01000193;
        h ^= (unsigned long)*s++;
    }
    return h;
}
```

The resulting hash is formatted as `%lx` and joined with the configured
`cache-root` directory to produce the cache file path.  The lock file is
the same path with `.lock` appended.

## Slot Lifecycle

A cache request goes through these phases, managed by `process_slot()`:

### 1. Open (`open_slot`)

Opens the cache file and reads the header.  The header contains the original
key followed by creation and expiry timestamps.  If the stored key matches the
current request key, `slot->match` is set to 1.

```c
static int open_slot(struct cache_slot *slot)
{
    slot->cache_fd = open(slot->cache_name, O_RDONLY);
    if (slot->cache_fd == -1)
        return errno;
    if (fstat(slot->cache_fd, &slot->cache_st))
        return errno;
    /* read header into slot->buf */
    return 0;
}
```

### 2. Check Match

If the file exists and the key matches, the code checks whether the entry
has expired based on the TTL:

```c
static int is_expired(struct cache_slot *slot)
{
    if (slot->ttl < 0)
        return 0;       /* negative TTL = never expires */
    return slot->cache_st.st_mtime + slot->ttl * 60 < time(NULL);
}
```

A TTL of `-1` means the entry never expires (used for `cache-static-ttl`).

### 3. Lock (`lock_slot`)

Creates the `.lock` file with `O_WRONLY | O_CREAT | O_EXCL` and writes the
header containing the key and timestamps.  If locking fails (another process
holds the lock), the stale cached content is served instead.

```c
static int lock_slot(struct cache_slot *slot)
{
    slot->lock_fd = open(slot->lock_name,
        O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (slot->lock_fd == -1)
        return errno;
    /* write header: key + creation timestamp */
    return 0;
}
```

### 4. Fill (`fill_slot`)

Redirects `stdout` to the lock file using `dup2()`, invokes the
`cache_fill_fn` callback to generate the page content, then restores `stdout`:

```c
static int fill_slot(struct cache_slot *slot)
{
    /* save original stdout */
    /* dup2(slot->lock_fd, STDOUT_FILENO) */
    slot->fn(slot->cbdata);
    /* restore original stdout */
    return 0;
}
```

### 5. Close and Rename

After filling, the lock file is atomically renamed to the cache file:

```c
if (rename(slot->lock_name, slot->cache_name))
    return errno;
```

This ensures readers never see a partially-written file.

### 6. Print (`print_slot`)

The cache file content (minus the header) is sent to `stdout`.  On Linux,
`sendfile()` is used for zero-copy output:

```c
static int print_slot(struct cache_slot *slot)
{
#ifdef HAVE_LINUX_SENDFILE
    off_t start = slot->keylen + 1;  /* skip header */
    sendfile(STDOUT_FILENO, slot->cache_fd, &start,
             slot->cache_st.st_size - start);
#else
    /* fallback: read()/write() loop */
#endif
}
```

## Process Slot State Machine

`process_slot()` implements a state machine combining all phases:

```
START → open_slot()
  ├── success + key match + not expired → print_slot() → DONE
  ├── success + key match + expired → lock_slot()
  │     ├── lock acquired → fill_slot() → close_slot() → open_slot() → print_slot()
  │     └── lock failed → print_slot() (serve stale)
  ├── success + key mismatch → lock_slot()
  │     ├── lock acquired → fill_slot() → close_slot() → open_slot() → print_slot()
  │     └── lock failed → fill_slot() (direct to stdout)
  └── open failed → lock_slot()
        ├── lock acquired → fill_slot() → close_slot() → open_slot() → print_slot()
        └── lock failed → fill_slot() (direct to stdout, no cache)
```

## Public API

```c
/* Process a request through the cache */
extern int cache_process(int size, const char *path, const char *key,
                         int ttl, cache_fill_fn fn, void *cbdata);

/* List all cache entries (for debugging/administration) */
extern int cache_ls(const char *path);

/* Hash a string using FNV-1 */
extern unsigned long hash_str(const char *str);
```

### `cache_process()`

Parameters:
- `size` — Maximum number of cache entries (from `cache-size`).  If `0`,
  caching is bypassed and `fn` is called directly.
- `path` — Cache root directory.
- `key` — Request identifier (derived from full URL + query string).
- `ttl` — Time-to-live in minutes.
- `fn` — Callback function that generates the page content.
- `cbdata` — Opaque data passed to the callback.

### `cache_ls()`

Scans the cache root directory and prints information about each cache entry
to `stdout`.  Used for administrative inspection.

## TTL Configuration Mapping

Different page types have different TTLs:

| Page Type | Config Directive | Default | Applied When |
|-----------|-----------------|---------|--------------|
| Repository list | `cache-root-ttl` | 5 min | `cmd->want_repo == 0` |
| Repo pages | `cache-repo-ttl` | 5 min | `cmd->want_repo == 1` and dynamic |
| Dynamic pages | `cache-dynamic-ttl` | 5 min | `cmd->want_vpath == 1` |
| Static content | `cache-static-ttl` | -1 (never) | SHA-referenced content |
| About pages | `cache-about-ttl` | 15 min | About/readme view |
| Snapshots | `cache-snapshot-ttl` | 5 min | Snapshot downloads |
| Scan results | `cache-scanrc-ttl` | 15 min | scan-path results |

Static content uses a TTL of `-1` because SHA-addressed content is
immutable — a given commit/tree/blob hash always refers to the same data.

## Cache Key Generation

The cache key is built from the complete query context in `cgit.c`:

```c
static const char *cache_key(void)
{
    return fmt("%s?%s?%s?%s?%s",
        ctx.qry.raw, ctx.env.http_host,
        ctx.env.https ? "1" : "0",
        ctx.env.authenticated ? "1" : "0",
        ctx.env.http_cookie ? ctx.env.http_cookie : "");
}
```

The key captures: raw query string, hostname, HTTPS state, authentication
state, and cookies.  This ensures that authenticated users get different
cache entries than unauthenticated users.

## Concurrency

The cache supports concurrent access from multiple CGI processes:

1. **Atomic writes**: Content is written to a `.lock` file first, then
   atomically renamed to the cache file.  Readers never see partial content.
2. **Non-blocking locks**: If a lock is already held, the process either
   serves stale cached content (if available) or generates content directly
   to stdout without caching.
3. **No deadlocks**: Lock files are `O_EXCL`, not `flock()`.  If a process
   crashes while holding a lock, the stale `.lock` file remains.  It is
   typically cleaned up by the next successful writer.

## Cache Directory Management

The cache root directory (`cache-root`, default `/var/cache/cgit`) must be
writable by the web server user.  Cache files are created with mode `0600`
(`S_IRUSR | S_IWUSR`).

There is no built-in cache eviction.  Old cache files persist until a new
request with the same hash replaces them.  Administrators should set up
periodic cleanup (e.g., a cron job) to purge expired files:

```bash
find /var/cache/cgit -type f -mmin +60 -delete
```

## Disabling the Cache

Set `cache-size=0` (the default).  When `size` is 0, `cache_process()` calls
the fill function directly, writing to stdout with no file I/O overhead:

```c
if (!size) {
    fn(cbdata);
    return 0;
}
```
