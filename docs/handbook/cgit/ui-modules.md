# cgit — UI Modules

## Overview

cgit's user interface is implemented as a collection of `ui-*.c` modules,
each responsible for rendering a specific page type.  All modules share
common infrastructure from `ui-shared.c` and `html.c`.

## Module Map

| Module | Page | Entry Function |
|--------|------|---------------|
| `ui-repolist.c` | Repository index | `cgit_print_repolist()` |
| `ui-summary.c` | Repository summary | `cgit_print_summary()` |
| `ui-log.c` | Commit log | `cgit_print_log()` |
| `ui-tree.c` | File/directory tree | `cgit_print_tree()` |
| `ui-blob.c` | File content | `cgit_print_blob()` |
| `ui-commit.c` | Commit details | `cgit_print_commit()` |
| `ui-diff.c` | Diff view | `cgit_print_diff()` |
| `ui-ssdiff.c` | Side-by-side diff | `cgit_ssdiff_*()` |
| `ui-patch.c` | Patch output | `cgit_print_patch()` |
| `ui-refs.c` | Branch/tag listing | `cgit_print_refs()` |
| `ui-tag.c` | Tag details | `cgit_print_tag()` |
| `ui-stats.c` | Statistics | `cgit_print_stats()` |
| `ui-atom.c` | Atom feed | `cgit_print_atom()` |
| `ui-plain.c` | Raw file serving | `cgit_print_plain()` |
| `ui-blame.c` | Blame view | `cgit_print_blame()` |
| `ui-clone.c` | HTTP clone | `cgit_clone_info/objects/head()` |
| `ui-snapshot.c` | Archive download | `cgit_print_snapshot()` |
| `ui-shared.c` | Common layout | (shared functions) |

## `ui-repolist.c` — Repository Index

Renders the main page listing all configured repositories.

### Functions

```c
void cgit_print_repolist(void)
```

### Features

- Sortable columns: Name, Description, Owner, Idle (age)
- Section grouping (based on `repo.section` or `section-from-path`)
- Pagination with configurable `max-repo-count`
- Age calculation via `read_agefile()` or ref modification time
- Optional filter by search query

### Sorting

```c
static int cmp_name(const void *a, const void *b);
static int cmp_section(const void *a, const void *b);
static int cmp_idle(const void *a, const void *b);
```

Sort field is selected by the `s` query parameter or `repository-sort`
directive.

### Age File Resolution

```c
static time_t read_agefile(const char *path)
{
    /* Try reading date from agefile content */
    /* Fall back to file mtime */
    /* Fall back to refs/ dir mtime */
}
```

### Pagination

```c
static void print_pager(int items, int pagelen, char *search, char *sort)
{
    /* Render page navigation links */
    /* [prev] 1 2 3 4 5 [next] */
}
```

## `ui-summary.c` — Repository Summary

Renders the overview page for a single repository.

### Functions

```c
void cgit_print_summary(void)
```

### Content

- Repository metadata table (description, owner, homepage, clone URLs)
- SPDX license detection from `LICENSES/` directory
- CODEOWNERS and MAINTAINERS file detection
- Badges display
- Branch listing (limited by `summary-branches`)
- Tag listing (limited by `summary-tags`)
- Recent commits (limited by `summary-log`)
- Snapshot download links
- README rendering (via about-filter)

### License Detection

```c
/* Scan for SPDX license identifiers */
/* Check LICENSES/ directory for .txt files */
/* Extract license names from filenames */
```

### README Priority

README files are tried in order of `repo.readme` entries:

1. `ref:README.md` — tracked file in a specific ref
2. `:README.md` — tracked file in HEAD
3. `/path/to/README.md` — file on disk

## `ui-log.c` — Commit Log

Renders a paginated list of commits.

### Functions

```c
void cgit_print_log(const char *tip, int ofs, int cnt,
                    char *grep, char *pattern, char *path,
                    int pager, int commit_graph, int commit_sort)
```

### Features

- Commit graph visualization (ASCII art)
- File change count per commit (when `enable-log-filecount=1`)
- Line count per commit (when `enable-log-linecount=1`)
- Grep/search within commit messages
- Path filtering (show commits affecting a specific path)
- Follow renames (when `enable-follow-links=1`)
- Pagination with next/prev links

### Commit Graph Colors

```c
static const char *column_colors_html[] = {
    "<span class='column1'>",
    "<span class='column2'>",
    "<span class='column3'>",
    "<span class='column4'>",
    "<span class='column5'>",
    "<span class='column6'>",
};
```

### Decorations

```c
static void show_commit_decorations(struct commit *commit)
{
    /* Display branch/tag labels next to commits */
    /* Uses git's decoration API */
}
```

## `ui-tree.c` — Tree View

Renders directory listings and file contents.

### Functions

```c
void cgit_print_tree(const char *rev, char *path)
```

### Directory Listing

For each entry in a tree object:

```c
/* For each tree entry */
switch (entry->mode) {
    case S_IFDIR:   /* directory → link to subtree */
    case S_IFREG:   /* regular file → link to blob */
    case S_IFLNK:   /* symlink → show target */
    case S_IFGITLINK: /* submodule → link to submodule */
}
```

### File Display

```c
static void print_text_buffer(const char *name, char *buf,
                               unsigned long size)
{
    /* Show file content with line numbers */
    /* Apply source filter if configured */
}

static void print_binary_buffer(char *buf, unsigned long size)
{
    /* Show "Binary file (N bytes)" message */
}
```

### Walk Tree Context

```c
struct walk_tree_context {
    char *curr_rev;
    char *match_path;
    int state;     /* 0=searching, 1=found, 2=printed */
};
```

The tree walker recursively descends into subdirectories to find the
requested path.

## `ui-blob.c` — Blob View

Displays individual file content or serves raw file data.

### Functions

```c
void cgit_print_blob(const char *hex, char *path,
                     const char *head, int file_only)
int cgit_ref_path_exists(const char *path, const char *ref, int file_only)
char *cgit_ref_read_file(const char *path, const char *ref,
                         unsigned long *size)
```

### MIME Detection

When serving raw content, MIME types are detected from:
1. The `mimetype.<ext>` configuration directives
2. The `mimetype-file` (Apache-style mime.types)
3. Default: `application/octet-stream`

## `ui-commit.c` — Commit View

Displays full commit details.

### Functions

```c
void cgit_print_commit(const char *rev, const char *prefix)
```

### Content

- Author and committer info (name, email, date)
- Commit subject and full message
- Parent commit links
- Git notes
- Commit decorations (branches, tags)
- Diffstat
- Full diff (unified or side-by-side)

### Notes Display

```c
/* Check for git notes */
struct strbuf notes = STRBUF_INIT;
format_display_notes(&commit->object.oid, &notes, ...);
if (notes.len) {
    html("<div class='notes-header'>Notes</div>");
    html("<div class='notes'>");
    html_txt(notes.buf);
    html("</div>");
}
```

## `ui-diff.c` — Diff View

Renders diffs between commits or trees.

### Functions

```c
void cgit_print_diff(const char *new_rev, const char *old_rev,
                     const char *prefix, int show_ctrls, int raw)
void cgit_print_diffstat(const struct object_id *old,
                         const struct object_id *new,
                         const char *prefix)
```

See [diff-engine.md](diff-engine.md) for detailed documentation.

## `ui-ssdiff.c` — Side-by-Side Diff

Renders two-column diff view with character-level highlighting.

### Functions

```c
void cgit_ssdiff_header_begin(void)
void cgit_ssdiff_header_end(void)
void cgit_ssdiff_footer(void)
```

See [diff-engine.md](diff-engine.md) for LCS algorithm details.

## `ui-patch.c` — Patch Output

Generates a downloadable patch file.

### Functions

```c
void cgit_print_patch(const char *new_rev, const char *old_rev,
                      const char *prefix)
```

Output is `text/plain` content suitable for `git apply`.  Uses Git's
`rev_info` and `log_tree_commit` to generate the patch.

## `ui-refs.c` — References View

Displays branches and tags with sorting.

### Functions

```c
void cgit_print_refs(void)
void cgit_print_branches(int max)
void cgit_print_tags(int max)
```

### Branch Display

Each branch row shows:
- Branch name (link to log)
- Idle time
- Author of last commit

### Tag Display

Each tag row shows:
- Tag name (link to tag)
- Idle time
- Author/tagger
- Download links (if snapshots enabled)

### Sorting

```c
static int cmp_branch_age(const void *a, const void *b);
static int cmp_tag_age(const void *a, const void *b);
static int cmp_branch_name(const void *a, const void *b);
static int cmp_tag_name(const void *a, const void *b);
```

Sort order is controlled by `branch-sort` (0=name, 1=age).

## `ui-tag.c` — Tag View

Displays details of a specific tag.

### Functions

```c
void cgit_print_tag(const char *revname)
```

### Content

For annotated tags:
- Tagger name and date
- Tag message
- Tagged object link

For lightweight tags:
- Redirects to the tagged object (commit, tree, or blob)

## `ui-stats.c` — Statistics View

Displays contributor statistics by period.

### Functions

```c
void cgit_print_stats(void)
```

### Periods

```c
struct cgit_period {
    const char *name;     /* "week", "month", "quarter", "year" */
    int max_periods;
    int count;
    /* accessor functions for period boundaries */
};
```

### Data Collection

```c
static void collect_stats(struct cgit_period *period)
{
    /* Walk commit log */
    /* Group commits by author and period */
    /* Count additions/deletions per period */
}
```

### Output

- Bar chart showing commits per period
- Author ranking table
- Sortable by commit count

## `ui-atom.c` — Atom Feed

Generates an Atom XML feed.

### Functions

```c
void cgit_print_atom(char *tip, char *path, int max)
```

### Output

```xml
<?xml version='1.0' encoding='utf-8'?>
<feed xmlns='http://www.w3.org/2005/Atom'>
  <title>repo - log</title>
  <updated>2024-01-01T00:00:00Z</updated>
  <entry>
    <title>commit subject</title>
    <updated>2024-01-01T00:00:00Z</updated>
    <author><name>Alice</name><email>alice@example.com</email></author>
    <id>urn:sha1:abc123</id>
    <link href='commit URL'/>
    <content type='text'>commit message</content>
  </entry>
</feed>
```

Limited by `max-atom-items` (default 10).

## `ui-plain.c` — Raw File Serving

Serves file content with proper MIME types.

### Functions

```c
void cgit_print_plain(void)
```

### Features

- MIME type detection by file extension
- Directory listing (HTML) when path is a tree
- Binary file serving with correct Content-Type
- Security: HTML serving gated by `enable-html-serving`

### Security

When `enable-html-serving=0` (default), HTML files are served as
`text/plain` to prevent XSS.

## `ui-blame.c` — Blame View

Displays line-by-line blame information.

### Functions

```c
void cgit_print_blame(void)
```

### Implementation

Uses Git's `blame_scoreboard` API:

```c
/* Set up blame scoreboard */
/* Walk file history */
/* For each line, emit: commit hash, author, line content */
```

### Output

Each line shows:
- Abbreviated commit hash (linked to commit view)
- Line number
- File content

Requires `enable-blame=1`.

## `ui-clone.c` — HTTP Clone Endpoints

Serves the smart HTTP clone protocol.

### Functions

```c
void cgit_clone_info(void)     /* GET info/refs */
void cgit_clone_objects(void)  /* GET objects/* */
void cgit_clone_head(void)     /* GET HEAD */
```

### `cgit_clone_info()`

Enumerates all refs and their SHA-1 hashes:

```c
static void print_ref_info(const char *refname,
                           const struct object_id *oid, ...)
{
    /* Output: sha1\trefname\n */
}
```

### `cgit_clone_objects()`

Serves loose objects and pack files from the object store.

### `cgit_clone_head()`

Returns the symbolic HEAD reference.

Requires `enable-http-clone=1` (default).

## `ui-snapshot.c` — Archive Downloads

See [snapshot-system.md](snapshot-system.md) for detailed documentation.

## `ui-shared.c` — Common Infrastructure

Provides shared layout and link generation used by all modules.

See [html-rendering.md](html-rendering.md) for detailed documentation.

### Key Functions

- Page skeleton: `cgit_print_docstart()`, `cgit_print_pageheader()`,
  `cgit_print_docend()`
- Links: `cgit_commit_link()`, `cgit_tree_link()`, `cgit_log_link()`, etc.
- URLs: `cgit_repourl()`, `cgit_fileurl()`, `cgit_pageurl()`
- Errors: `cgit_print_error_page()`
