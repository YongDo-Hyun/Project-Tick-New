# cgit — Architecture

## High-Level Component Map

```
┌──────────────────────────────────────────────────────────────┐
│                          cgit.c                               │
│  constructor_environment() [__attribute__((constructor))]      │
│  prepare_context() → config_cb() → querystring_cb()           │
│  authenticate_cookie() → process_request() → main()           │
├──────────────────────────────────────────────────────────────┤
│                     Command Dispatcher                        │
│                         cmd.c                                 │
│  cgit_get_cmd() → static cmds[] table (23 entries)            │
│  struct cgit_cmd { name, fn, want_repo, want_vpath, is_clone }│
├──────────┬───────────┬───────────┬───────────────────────────┤
│ UI Layer │  Caching  │  Filters  │  HTTP Clone               │
│ ui-*.c   │  cache.c  │  filter.c │  ui-clone.c               │
│ (17 mods)│  cache.h  │           │                           │
├──────────┴───────────┴───────────┴───────────────────────────┤
│                     Core Utilities                            │
│  shared.c     — global vars, repo mgmt, diff wrappers        │
│  parsing.c    — cgit_parse_commit(), cgit_parse_tag(),        │
│                 cgit_parse_url()                               │
│  html.c       — entity escaping, URL encoding, form helpers   │
│  configfile.c — line-oriented name=value parser               │
│  scan-tree.c  — filesystem repository discovery               │
├──────────────────────────────────────────────────────────────┤
│                    Vendored git library                        │
│  git/  — full Git 2.46.0 source; linked via cgit.mk           │
│  Provides: object store, diff engine (xdiff), refs, revwalk,  │
│            archive, notes, commit graph, blame, packfile       │
└──────────────────────────────────────────────────────────────┘
```

## Global State

cgit uses a single global variable to carry all request state:

```c
/* shared.c */
struct cgit_repolist cgit_repolist;   /* Array of all known repositories */
struct cgit_context ctx;               /* Current request context */
```

### `struct cgit_context`

```c
struct cgit_context {
    struct cgit_environment env;   /* CGI env vars (HTTP_HOST, QUERY_STRING, etc.) */
    struct cgit_query       qry;   /* Parsed URL/query parameters */
    struct cgit_config      cfg;   /* All global config directives */
    struct cgit_repo       *repo;  /* Pointer into cgit_repolist.repos[] or NULL */
    struct cgit_page        page;  /* HTTP response metadata (mimetype, status, etag) */
};
```

### `struct cgit_environment`

Populated by `prepare_context()` via `getenv()`:

```c
struct cgit_environment {
    const char *cgit_config;      /* $CGIT_CONFIG (default: /etc/cgitrc) */
    const char *http_host;        /* $HTTP_HOST */
    const char *https;            /* $HTTPS ("on" if TLS) */
    const char *no_http;          /* $NO_HTTP (non-NULL → CLI mode) */
    const char *path_info;        /* $PATH_INFO */
    const char *query_string;     /* $QUERY_STRING */
    const char *request_method;   /* $REQUEST_METHOD */
    const char *script_name;      /* $SCRIPT_NAME */
    const char *server_name;      /* $SERVER_NAME */
    const char *server_port;      /* $SERVER_PORT */
    const char *http_cookie;      /* $HTTP_COOKIE */
    const char *http_referer;     /* $HTTP_REFERER */
    unsigned int content_length;  /* $CONTENT_LENGTH */
    int authenticated;            /* Set by auth filter (0 or 1) */
};
```

### `struct cgit_page`

Controls HTTP response headers:

```c
struct cgit_page {
    time_t modified;       /* Last-Modified header */
    time_t expires;        /* Expires header */
    size_t size;           /* Content-Length (0 = omit) */
    const char *mimetype;  /* Content-Type (default: "text/html") */
    const char *charset;   /* charset param (default: "UTF-8") */
    const char *filename;  /* Content-Disposition filename */
    const char *etag;      /* ETag header value */
    const char *title;     /* HTML <title> */
    int status;            /* HTTP status code (0 = 200) */
    const char *statusmsg; /* HTTP status message */
};
```

## Request Lifecycle — Detailed

### Phase 1: Pre-main Initialization

```c
__attribute__((constructor))
static void constructor_environment()
{
    setenv("GIT_CONFIG_NOSYSTEM", "1", 1);
    setenv("GIT_ATTR_NOSYSTEM", "1", 1);
    unsetenv("HOME");
    unsetenv("XDG_CONFIG_HOME");
}
```

This runs before `main()` on every invocation.  It prevents Git from loading
`/etc/gitconfig`, `~/.gitconfig`, or any `$XDG_CONFIG_HOME/git/config`, ensuring
complete isolation from the host system's Git configuration.

### Phase 2: Context Preparation

`prepare_context()` zero-initializes `ctx` and sets every configuration field
to its default value.  Key defaults:

| Field | Default |
|-------|---------|
| `cfg.cache_size` | `0` (disabled) |
| `cfg.cache_root` | `CGIT_CACHE_ROOT` (`/var/cache/cgit`) |
| `cfg.cache_repo_ttl` | `5` minutes |
| `cfg.cache_root_ttl` | `5` minutes |
| `cfg.cache_static_ttl` | `-1` (never expires) |
| `cfg.max_repo_count` | `50` |
| `cfg.max_commit_count` | `50` |
| `cfg.max_msg_len` | `80` |
| `cfg.max_repodesc_len` | `80` |
| `cfg.enable_http_clone` | `1` |
| `cfg.enable_index_owner` | `1` |
| `cfg.enable_tree_linenumbers` | `1` |
| `cfg.summary_branches` | `10` |
| `cfg.summary_log` | `10` |
| `cfg.summary_tags` | `10` |
| `cfg.difftype` | `DIFF_UNIFIED` |
| `cfg.robots` | `"index, nofollow"` |
| `cfg.root_title` | `"Git repository browser"` |

The function also reads all CGI environment variables and sets
`page.mimetype = "text/html"`, `page.charset = PAGE_ENCODING` (`"UTF-8"`).

### Phase 3: Configuration Parsing

```c
parse_configfile(ctx.env.cgit_config, config_cb);
```

`parse_configfile()` (in `configfile.c`) opens the file, reads lines of the
form `name=value`, skips comments (`#` and `;`), and calls the callback for each
directive.  It supports recursive `include=` directives up to 8 levels deep.

`config_cb()` (in `cgit.c`) is a ~200-line chain of `if/else if` blocks that
maps directive names to `ctx.cfg.*` fields.  When `repo.url=` is encountered,
`cgit_add_repo()` allocates a new repository entry; subsequent `repo.*`
directives configure that entry via `repo_config()`.

Special directive: `scan-path=` triggers immediate filesystem scanning via
`scan_tree()` or `scan_projects()`, or via a cached repolist file if
`cache-size > 0`.

### Phase 4: Query String Parsing

```c
http_parse_querystring(ctx.qry.raw, querystring_cb);
```

`querystring_cb()` maps short parameter names to `ctx.qry.*` fields:

| Parameter | Field | Purpose |
|-----------|-------|---------|
| `r` | `qry.repo` | Repository URL |
| `p` | `qry.page` | Page name |
| `url` | `qry.url` | Combined repo/page/path |
| `h` | `qry.head` | Branch/ref |
| `id` | `qry.oid` | Object ID |
| `id2` | `qry.oid2` | Second object ID (for diffs) |
| `ofs` | `qry.ofs` | Pagination offset |
| `q` | `qry.search` | Search query |
| `qt` | `qry.grep` | Search type |
| `path` | `qry.path` | File path |
| `name` | `qry.name` | Snapshot filename |
| `dt` | `qry.difftype` | Diff type (0/1/2) |
| `context` | `qry.context` | Diff context lines |
| `ignorews` | `qry.ignorews` | Ignore whitespace |
| `follow` | `qry.follow` | Follow renames |
| `showmsg` | `qry.showmsg` | Show full messages |
| `s` | `qry.sort` | Sort order |
| `period` | `qry.period` | Stats period |

The `url=` parameter receives special processing via `cgit_parse_url()` (in
`parsing.c`), which iteratively splits the URL at `/` characters, looking for
the longest prefix that matches a known repository URL.

### Phase 5: Authentication

`authenticate_cookie()` checks three cases:

1. **No auth filter** → set `ctx.env.authenticated = 1` and return.
2. **POST to login page** → call `authenticate_post()`, which reads up to
   `MAX_AUTHENTICATION_POST_BYTES` (4096) from stdin, pipes it to the auth
   filter with function `"authenticate-post"`, and exits.
3. **Normal request** → invoke auth filter with function
   `"authenticate-cookie"`.  The filter's exit code becomes
   `ctx.env.authenticated`.

The auth filter receives 12 arguments:

```
function, cookie, method, query_string, http_referer,
path_info, http_host, https, repo, page, fullurl, loginurl
```

### Phase 6: Cache Envelope

If `ctx.cfg.cache_size > 0`, the request is wrapped in `cache_process()`:

```c
cache_process(ctx.cfg.cache_size, ctx.cfg.cache_root,
              cache_key, ttl, fill_fn);
```

This constructs a filename from the FNV-1 hash of the cache key, attempts to
open an existing slot, verifies the key matches, checks expiry, and either
serves cached content or locks and fills a new slot.  See the Caching System
document for full details.

### Phase 7: Command Dispatch

```c
cmd = cgit_get_cmd();
```

`cgit_get_cmd()` (in `cmd.c`) performs a linear scan of the static `cmds[]`
table:

```c
static struct cgit_cmd cmds[] = {
    def_cmd(HEAD, 1, 0, 1),
    def_cmd(atom, 1, 0, 0),
    def_cmd(about, 0, 0, 0),
    def_cmd(blame, 1, 1, 0),
    def_cmd(blob, 1, 0, 0),
    def_cmd(cla, 0, 0, 0),
    def_cmd(commit, 1, 1, 0),
    def_cmd(coc, 0, 0, 0),
    def_cmd(diff, 1, 1, 0),
    def_cmd(info, 1, 0, 1),
    def_cmd(log, 1, 1, 0),
    def_cmd(ls_cache, 0, 0, 0),
    def_cmd(objects, 1, 0, 1),
    def_cmd(patch, 1, 1, 0),
    def_cmd(plain, 1, 0, 0),
    def_cmd(rawdiff, 1, 1, 0),
    def_cmd(refs, 1, 0, 0),
    def_cmd(repolist, 0, 0, 0),
    def_cmd(snapshot, 1, 0, 0),
    def_cmd(stats, 1, 1, 0),
    def_cmd(summary, 1, 0, 0),
    def_cmd(tag, 1, 0, 0),
    def_cmd(tree, 1, 1, 0),
};
```

The `def_cmd` macro expands to `{#name, name##_fn, want_repo, want_vpath, is_clone}`.

Default page if none specified:
- With a repository → `"summary"`
- Without a repository → `"repolist"`

### Phase 8: Repository Preparation

If `cmd->want_repo` is set:

1. `prepare_repo_env()` calls `setenv("GIT_DIR", ctx.repo->path, 1)`,
   `setup_git_directory_gently()`, and `load_display_notes()`.
2. `prepare_repo_cmd()` resolves the default branch (via `guess_defbranch()`
   which checks `HEAD` → `refs/heads/*`), resolves the requested head to an OID,
   sorts submodules, chooses the README file, and sets the page title.

### Phase 9: Page Rendering

The handler function (`cmd->fn()`) is called.  Most handlers follow this
pattern:

```c
cgit_print_layout_start();  /* HTTP headers + HTML doctype + header + tabs */
/* ... page-specific content ... */
cgit_print_layout_end();    /* footer + closing tags */
```

`cgit_print_layout_start()` calls:
- `cgit_print_http_headers()` — Content-Type, Last-Modified, Expires, ETag
- `cgit_print_docstart()` — `<!DOCTYPE html>`, `<html>`, CSS/JS includes
- `cgit_print_pageheader()` — header table, navigation tabs, breadcrumbs

## Module Dependency Graph

```
cgit.c ──→ cmd.c ──→ ui-*.c (all modules)
  │          │
  │          └──→ cache.c
  │
  ├──→ configfile.c
  ├──→ scan-tree.c ──→ configfile.c
  ├──→ ui-shared.c ──→ html.c
  ├──→ ui-stats.c
  ├──→ ui-blob.c
  ├──→ ui-summary.c
  └──→ filter.c

ui-commit.c ──→ ui-diff.c ──→ ui-ssdiff.c
ui-summary.c ──→ ui-log.c, ui-refs.c, ui-blob.c, ui-plain.c
ui-log.c ──→ ui-shared.c
All ui-*.c ──→ html.c, ui-shared.c
```

## The `struct cgit_cmd` Pattern

Each command in `cmd.c` is defined as a static function that wraps the
corresponding UI module:

```c
static void log_fn(void)
{
    cgit_print_log(ctx.qry.oid, ctx.qry.ofs, ctx.cfg.max_commit_count,
                   ctx.qry.grep, ctx.qry.search, ctx.qry.path, 1,
                   ctx.repo->enable_commit_graph,
                   ctx.repo->commit_sort);
}
```

The thin wrapper pattern means all context is accessed via the global `ctx`
struct, and the wrapper simply extracts the relevant fields and passes them to
the module function.

## Repository List Management

The `cgit_repolist` global is a dynamically-growing array:

```c
struct cgit_repolist {
    int length;              /* Allocated capacity */
    int count;               /* Number of repos */
    struct cgit_repo *repos; /* Array */
};
```

`cgit_add_repo()` doubles the array capacity when needed (starting from 8).
Each new repo inherits defaults from `ctx.cfg.*` (snapshots, feature flags,
filters, etc.).

`cgit_get_repoinfo()` performs a linear scan (O(n)) to find a repo by URL.
Ignored repos (`repo->ignore == 1`) are skipped.

## Build System

The build works in two stages:

1. **Git build** — `make` in the top-level `cgit/` directory delegates to
   `make -C git -f ../cgit.mk` which includes Git's own `Makefile`.

2. **cgit link** — `cgit.mk` lists all cgit object files (`CGIT_OBJ_NAMES`),
   compiles them with `CGIT_CFLAGS` (which embeds `CGIT_CONFIG`,
   `CGIT_SCRIPT_NAME`, `CGIT_CACHE_ROOT` as string literals), and links them
   against Git's `libgit.a`.

Lua support is auto-detected via `pkg-config` (checking `luajit`, `lua`,
`lua5.2`, `lua5.1` in order).  Define `NO_LUA=1` to build without Lua.
Linux systems get `HAVE_LINUX_SENDFILE` which enables the `sendfile()` syscall
in the cache layer.

## Thread Safety

cgit runs as a **single-process CGI** — one process per HTTP request.  There is
no multi-threading.  All global state (`ctx`, `cgit_repolist`, the static
`diffbuf` in `shared.c`, the static format buffers in `html.c`) is safe because
each process is fully isolated.

The `fmt()` function in `html.c` uses a ring buffer of 8 static buffers
(`static char buf[8][1024]`) to allow up to 8 nested `fmt()` calls in a single
expression.  The `bufidx` rotates via `bufidx = (bufidx + 1) & 7`.

## Error Handling

The codebase uses three assertion-style helpers from `shared.c`:

```c
int chk_zero(int result, char *msg);         /* die if result != 0 */
int chk_positive(int result, char *msg);     /* die if result <= 0 */
int chk_non_negative(int result, char *msg); /* die if result < 0 */
```

For user-facing errors, `cgit_print_error_page()` sets HTTP status, prints
headers, renders the page skeleton, and displays the error message.

## Type System

cgit uses three enums defined in `cgit.h`:

```c
typedef enum {
    DIFF_UNIFIED, DIFF_SSDIFF, DIFF_STATONLY
} diff_type;

typedef enum {
    ABOUT, COMMIT, SOURCE, EMAIL, AUTH, OWNER
} filter_type;
```

And three function pointer typedefs:

```c
typedef void (*configfn)(const char *name, const char *value);
typedef void (*filepair_fn)(struct diff_filepair *pair);
typedef void (*linediff_fn)(char *line, int len);
```
