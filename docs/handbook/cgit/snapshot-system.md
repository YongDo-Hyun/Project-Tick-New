# cgit — Snapshot System

## Overview

cgit can generate downloadable source archives (snapshots) from any git
reference.  Supported formats include tar, compressed tar variants, and zip.
The snapshot system validates requests against a configured format mask and
delegates archive generation to the git archive API.

Source file: `ui-snapshot.c`, `ui-snapshot.h`.

## Snapshot Format Table

All supported formats are defined in `cgit_snapshot_formats[]`:

```c
const struct cgit_snapshot_format cgit_snapshot_formats[] = {
    { ".zip",      "application/x-zip",     write_zip_archive,     0x01 },
    { ".tar.gz",   "application/x-gzip",    write_tar_gzip_archive, 0x02 },
    { ".tar.bz2",  "application/x-bzip2",   write_tar_bzip2_archive, 0x04 },
    { ".tar",      "application/x-tar",     write_tar_archive,     0x08 },
    { ".tar.xz",   "application/x-xz",      write_tar_xz_archive,  0x10 },
    { ".tar.zst",  "application/x-zstd",    write_tar_zstd_archive, 0x20 },
    { ".tar.lz",   "application/x-lzip",    write_tar_lzip_archive, 0x40 },
    { NULL }
};
```

### Format Structure

```c
struct cgit_snapshot_format {
    const char *suffix;        /* file extension */
    const char *mimetype;      /* HTTP Content-Type */
    write_archive_fn_t fn;     /* archive writer function */
    int bit;                   /* bitmask flag */
};
```

### Format Bitmask

Each format has a power-of-two bit value.  The `snapshots` configuration
directive sets a bitmask by OR-ing the bits of enabled formats:

| Suffix | Bit | Hex |
|--------|-----|-----|
| `.zip` | 0x01 | 1 |
| `.tar.gz` | 0x02 | 2 |
| `.tar.bz2` | 0x04 | 4 |
| `.tar` | 0x08 | 8 |
| `.tar.xz` | 0x10 | 16 |
| `.tar.zst` | 0x20 | 32 |
| `.tar.lz` | 0x40 | 64 |
| all | 0x7F | 127 |

### Parsing Snapshot Configuration

`cgit_parse_snapshots_mask()` in `shared.c` converts the configuration
string to a bitmask:

```c
int cgit_parse_snapshots_mask(const char *str)
{
    int mask = 0;
    /* for each word in str */
    /* compare against cgit_snapshot_formats[].suffix */
    /* if match, mask |= format->bit */
    /* "all" enables all formats */
    return mask;
}
```

## Snapshot Request Processing

### Entry Point: `cgit_print_snapshot()`

```c
void cgit_print_snapshot(const char *head, const char *hex,
                         const char *prefix, const char *filename,
                         int snapshots)
```

Parameters:
- `head` — Branch/tag reference
- `hex` — Commit SHA
- `prefix` — Archive prefix (directory name within archive)
- `filename` — Requested filename (e.g., `myrepo-v1.0.tar.gz`)
- `snapshots` — Enabled format bitmask

### Reference Resolution: `get_ref_from_filename()`

Decomposes the requested filename into a reference and format:

```c
static const struct cgit_snapshot_format *get_ref_from_filename(
    const char *filename, char **ref)
{
    /* for each format suffix */
    /*   if filename ends with suffix */
    /*     extract the part before the suffix as the ref */
    /*     return the matching format */
    /* strip repo prefix if present */
}
```

Example decomposition:
- `myrepo-v1.0.tar.gz` → ref=`v1.0`, format=`.tar.gz`
- `myrepo-main.zip` → ref=`main`, format=`.zip`
- `myrepo-abc1234.tar.xz` → ref=`abc1234`, format=`.tar.xz`

The prefix `myrepo-` is the `snapshot-prefix` (defaults to the repo basename).

### Validation

Before generating an archive, the function validates:

1. **Format enabled**: The format's bit must be set in the snapshot mask
2. **Reference exists**: The ref must resolve to a valid git object
3. **Object type**: Must be a commit, tag, or tree

### Archive Generation: `write_archive_type()`

```c
static int write_archive_type(const char *format, const char *hex,
                               const char *prefix)
{
    struct archiver_args args;
    memset(&args, 0, sizeof(args));
    args.base = prefix;  /* directory prefix in archive */
    /* resolve hex to tree object */
    /* call write_archive() from libgit */
}
```

The actual archive creation is delegated to Git's `write_archive()` API,
which handles tar and zip generation natively.

### Compression Pipeline

For compressed formats, the archive data is piped through compression:

```c
static int write_tar_gzip_archive(/* ... */)
{
    /* pipe tar output through gzip compression */
}

static int write_tar_bzip2_archive(/* ... */)
{
    /* pipe tar output through bzip2 compression */
}

static int write_tar_xz_archive(/* ... */)
{
    /* pipe tar output through xz compression */
}

static int write_tar_zstd_archive(/* ... */)
{
    /* pipe tar output through zstd compression */
}

static int write_tar_lzip_archive(/* ... */)
{
    /* pipe tar output through lzip compression */
}
```

## HTTP Response

Snapshot responses include:

```
Content-Type: application/x-gzip
Content-Disposition: inline; filename="myrepo-v1.0.tar.gz"
```

The `Content-Disposition` header triggers a file download in browsers with
the correct filename.

## Snapshot Links

Snapshot links on repository pages are generated by `ui-shared.c`:

```c
void cgit_print_snapshot_links(const char *repo, const char *head,
                               const char *hex, int snapshots)
{
    for (f = cgit_snapshot_formats; f->suffix; f++) {
        if (!(snapshots & f->bit))
            continue;
        /* generate link: repo/snapshot/prefix-ref.suffix */
    }
}
```

These links appear on the summary page and optionally in the log view.

## Snapshot Prefix

The archive prefix (directory name inside the archive) is determined by:

1. `repo.snapshot-prefix` if set
2. Otherwise, the repository basename

For a request like `myrepo-v1.0.tar.gz`, the archive contains files under
`myrepo-v1.0/`.

## Signature Detection

cgit can detect and display signature files alongside snapshots.  When a
file matching `<snapshot-name>.asc` or `<snapshot-name>.sig` exists in the
repository, a signature link is shown next to the snapshot download.

## Configuration

| Directive | Default | Description |
|-----------|---------|-------------|
| `snapshots` | (none) | Space-separated list of enabled suffixes |
| `repo.snapshots` | (inherited) | Per-repo override |
| `repo.snapshot-prefix` | (basename) | Per-repo archive prefix |
| `cache-snapshot-ttl` | 5 min | Cache TTL for snapshot pages |

### Enabling Snapshots

```ini
# Global: enable tar.gz and zip for all repos
snapshots=tar.gz zip

# Per-repo: enable all formats
repo.url=myrepo
repo.snapshots=all

# Per-repo: disable snapshots
repo.url=internal-tools
repo.snapshots=
```

## Security Considerations

- Snapshots are generated on-the-fly from git objects, so they always reflect
  the repository's current state
- Large repositories can produce large archives — consider enabling caching
  and setting appropriate `max-blob-size` limits
- Snapshot requests for non-existent refs return a 404 error page
- The snapshot filename is sanitized to prevent path traversal
