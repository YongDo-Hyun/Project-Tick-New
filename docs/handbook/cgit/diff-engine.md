# cgit — Diff Engine

## Overview

cgit's diff engine renders differences between commits, trees, and blobs.
It supports three diff modes: unified, side-by-side, and stat-only.  The
engine leverages libgit's internal diff machinery and adds HTML rendering on
top.

Source files: `ui-diff.c`, `ui-diff.h`, `ui-ssdiff.c`, `ui-ssdiff.h`,
`shared.c` (diff helpers).

## Diff Types

```c
#define DIFF_UNIFIED   0   /* traditional unified diff */
#define DIFF_SSDIFF    1   /* side-by-side diff */
#define DIFF_STATONLY  2   /* only show diffstat */
```

The diff type is selected by the `ss` query parameter or the
`side-by-side-diffs` configuration directive.

## Diffstat

### File Info Structure

```c
struct fileinfo {
    char status;           /* 'A'dd, 'D'elete, 'M'odify, 'R'ename, etc. */
    unsigned long old_size;
    unsigned long new_size;
    int binary;
    struct object_id old_oid;  /* old blob SHA */
    struct object_id new_oid;  /* new blob SHA */
    unsigned short old_mode;
    unsigned short new_mode;
    char *old_path;
    char *new_path;
    int added;             /* lines added */
    int removed;           /* lines removed */
};
```

### Collecting File Changes: `inspect_filepair()`

For each changed file in a commit, `inspect_filepair()` records the change
information:

```c
static void inspect_filepair(struct diff_filepair *pair)
{
    /* populate a fileinfo entry from the diff_filepair */
    files++;
    switch (pair->status) {
    case DIFF_STATUS_ADDED:
        info->status = 'A';
        break;
    case DIFF_STATUS_DELETED:
        info->status = 'D';
        break;
    case DIFF_STATUS_MODIFIED:
        info->status = 'M';
        break;
    case DIFF_STATUS_RENAMED:
        info->status = 'R';
        /* old_path and new_path differ */
        break;
    case DIFF_STATUS_COPIED:
        info->status = 'C';
        break;
    /* ... */
    }
}
```

### Rendering Diffstat: `cgit_print_diffstat()`

```c
void cgit_print_diffstat(const struct object_id *old,
                         const struct object_id *new,
                         const char *prefix)
```

Renders an HTML table showing changed files with bar graphs:

```html
<table summary='diffstat' class='diffstat'>
  <tr>
    <td class='mode'>M</td>
    <td class='upd'><a href='...'>src/main.c</a></td>
    <td class='right'>42</td>
    <td class='graph'>
      <span class='add' style='width: 70%'></span>
      <span class='rem' style='width: 30%'></span>
    </td>
  </tr>
  ...
  <tr class='total'>
    <td colspan='3'>5 files changed, 120 insertions, 45 deletions</td>
  </tr>
</table>
```

The bar graph width is calculated proportionally to the maximum changed
lines across all files.

## Unified Diff

### `cgit_print_diff()`

The main diff rendering function:

```c
void cgit_print_diff(const char *new_rev, const char *old_rev,
                     const char *prefix, int show_ctrls, int raw)
```

Parameters:
- `new_rev` — New commit SHA
- `old_rev` — Old commit SHA (optional; defaults to parent)
- `prefix` — Path prefix filter (show only diffs under this path)
- `show_ctrls` — Show diff controls (diff type toggle buttons)
- `raw` — Output raw diff without HTML wrapping

### Diff Controls

When `show_ctrls=1`, diff mode toggle buttons are rendered:

```html
<div class='cgit-panel'>
  <b>Diff options</b>
  <form method='get' action='...'>
    <select name='dt'>
      <option value='0'>unified</option>
      <option value='1'>ssdiff</option>
      <option value='2'>stat only</option>
    </select>
    <input type='submit' value='Go'/>
  </form>
</div>
```

### Filepair Callback: `filepair_cb()`

For each changed file, `filepair_cb()` renders the diff:

```c
static void filepair_cb(struct diff_filepair *pair)
{
    /* emit file header */
    htmlf("<div class='head'>%s</div>", pair->one->path);
    /* set up diff options */
    xdiff_opts.ctxlen = ctx.qry.context ?: 3;
    /* run the diff and emit line-by-line output */
    /* each line gets a CSS class: .add, .del, or .ctx */
}
```

### Hunk Headers

```c
void cgit_print_diff_hunk_header(int oldofs, int oldcnt,
                                  int newofs, int newcnt,
                                  const char *func)
```

Renders hunk headers as:

```html
<div class='hunk'>@@ -oldofs,oldcnt +newofs,newcnt @@ func</div>
```

### Line Rendering

Each diff line is rendered with a status prefix and CSS class:

| Line Type | CSS Class | Prefix |
|-----------|----------|--------|
| Added | `.add` | `+` |
| Removed | `.del` | `-` |
| Context | `.ctx` | ` ` |
| Hunk header | `.hunk` | `@@` |

## Side-by-Side Diff (`ui-ssdiff.c`)

The side-by-side diff view renders old and new versions in adjacent columns.

### LCS Algorithm

`ui-ssdiff.c` implements a Longest Common Subsequence (LCS) algorithm to
align lines between old and new versions:

```c
/* LCS computation for line alignment */
static int *lcs(char *a, int an, char *b, int bn)
{
    int *prev, *curr;
    /* dynamic programming: build LCS table */
    prev = calloc(bn + 1, sizeof(int));
    curr = calloc(bn + 1, sizeof(int));
    for (int i = 1; i <= an; i++) {
        for (int j = 1; j <= bn; j++) {
            if (a[i-1] == b[j-1])
                curr[j] = prev[j-1] + 1;
            else
                curr[j] = MAX(prev[j], curr[j-1]);
        }
        SWAP(prev, curr);
    }
    return prev;
}
```

### Deferred Lines

Side-by-side rendering uses a deferred output model:

```c
struct deferred_lines {
    int line_no;
    char *line;
    struct deferred_lines *next;
};
```

Lines are collected and paired before output.  For modified lines, the LCS
algorithm identifies character-level changes and highlights them with
`<span class='add'>` or `<span class='del'>` within each line.

### Tab Expansion

```c
static char *replace_tabs(char *line)
```

Tabs are expanded to spaces for proper column alignment in side-by-side
view.  The tab width is 8 characters.

### Rendering

Side-by-side output uses a two-column `<table>`:

```html
<table class='ssdiff'>
  <tr>
    <td class='lineno'><a>42</a></td>
    <td class='del'>old line content</td>
    <td class='lineno'><a>42</a></td>
    <td class='add'>new line content</td>
  </tr>
</table>
```

Changed characters within a line are highlighted with inline spans.

## Low-Level Diff Helpers (`shared.c`)

### Tree Diff

```c
void cgit_diff_tree(const struct object_id *old_oid,
                    const struct object_id *new_oid,
                    filepair_fn fn, const char *prefix,
                    int renamelimit)
```

Computes the diff between two tree objects (typically from two commits).
Calls `fn` for each changed file pair.  `renamelimit` controls rename
detection threshold.

### Commit Diff

```c
void cgit_diff_commit(struct commit *commit, filepair_fn fn,
                      const char *prefix)
```

Diffs a commit against its first parent.  For root commits (no parent),
diffs against an empty tree.

### File Diff

```c
void cgit_diff_files(const struct object_id *old_oid,
                     const struct object_id *new_oid,
                     unsigned long *old_size,
                     unsigned long *new_size,
                     int *binary, int context,
                     int ignorews, linediff_fn fn)
```

Performs a line-level diff between two blobs.  The `linediff_fn` callback is
invoked for each output line (add/remove/context).

## Diff in Context: Commit View

`ui-commit.c` uses the diff engine to show changes in commit view:

```c
void cgit_print_commit(const char *rev, const char *prefix)
{
    /* ... commit metadata ... */
    cgit_print_diff(ctx.qry.sha1, info->parent_sha1, prefix, 0, 0);
}
```

## Diff in Context: Log View

`ui-log.c` can optionally show per-commit diffstats:

```c
if (ctx.cfg.enable_log_filecount) {
    cgit_diff_commit(commit, inspect_filepair, NULL);
    /* display changed files count, added/removed */
}
```

## Binary Detection

Files are marked as binary when diffing if the content contains null bytes
or exceeds the configured max-blob-size.  Binary files are shown as:

```
Binary files differ
```

No line-level diff is performed for binary content.

## Diff Configuration

| Directive | Default | Effect |
|-----------|---------|--------|
| `side-by-side-diffs` | 0 | Default diff type |
| `renamelimit` | -1 | Rename detection limit |
| `max-blob-size` | 0 | Max blob size for display |
| `enable-log-filecount` | 0 | Show file counts in log |
| `enable-log-linecount` | 0 | Show line counts in log |

## Raw Diff Output

The `rawdiff` command outputs a plain-text unified diff without HTML
wrapping, suitable for piping or downloading:

```c
static void cmd_rawdiff(struct cgit_context *ctx)
{
    ctx->page.mimetype = "text/plain";
    cgit_print_diff(ctx->qry.sha1, ctx->qry.sha2,
                    ctx->qry.path, 0, 1 /* raw */);
}
```
