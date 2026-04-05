# cgit — URL Routing and Request Dispatch

## Overview

cgit supports two URL schemes: virtual-root (path-based) and query-string.
Incoming requests are parsed into a `cgit_query` structure and dispatched to
one of 23 command handlers via a function pointer table.

Source files: `cgit.c` (querystring parsing, routing), `parsing.c`
(`cgit_parse_url`), `cmd.c` (command table).

## URL Schemes

### Virtual Root (Path-Based)

When `virtual-root` is configured, URLs use clean paths:

```
/cgit/                           → repository list
/cgit/repo.git/                  → summary
/cgit/repo.git/log/              → log (default branch)
/cgit/repo.git/log/main/path    → log for path on branch main
/cgit/repo.git/tree/v1.0/src/   → tree view at tag v1.0
/cgit/repo.git/commit/?id=abc   → commit view
```

The path after the virtual root is passed in `PATH_INFO` and parsed by
`cgit_parse_url()`.

### Query-String (CGI)

Without virtual root, all parameters are passed in the query string:

```
/cgit.cgi?url=repo.git/log/main/path&ofs=50
```

## Query Structure

All parsed parameters are stored in `ctx.qry`:

```c
struct cgit_query {
    char *raw;         /* raw URL / PATH_INFO */
    char *repo;        /* repository URL */
    char *page;        /* page/command name */
    char *search;      /* search string */
    char *grep;        /* grep pattern */
    char *head;        /* branch reference */
    char *sha1;        /* object SHA-1 */
    char *sha2;        /* second SHA-1 (for diffs) */
    char *path;        /* file/dir path within repo */
    char *name;        /* snapshot name / ref name */
    char *url;         /* combined URL path */
    char *mimetype;    /* requested MIME type */
    char *etag;        /* ETag from client */
    int nohead;        /* suppress header */
    int ofs;           /* pagination offset */
    int has_symref;    /* path contains a symbolic ref */
    int has_sha1;      /* explicit SHA was given */
    int has_dot;       /* path contains '..' */
    int ignored;       /* request should be ignored */
    char *sort;        /* sort field */
    int showmsg;       /* show full commit message */
    int ssdiff;        /* side-by-side diff */
    int show_all;      /* show all items */
    int context;       /* diff context lines */
    int follow;        /* follow renames */
    int log_hierarchical_threading;
};
```

## URL Parsing: `cgit_parse_url()`

In `parsing.c`, the URL is decomposed into repo, page, and path:

```c
void cgit_parse_url(const char *url)
{
    /* Step 1: try progressively longer prefixes as repo URLs */
    /* For each '/' in the URL, check if the prefix matches a repo */
    
    for (p = strchr(url, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        repo = cgit_get_repoinfo(url);
        *p = '/';
        if (repo) {
            ctx.qry.repo = xstrdup(url_prefix);
            ctx.repo = repo;
            url = p + 1;  /* remaining part */
            break;
        }
    }
    /* if no '/' found, try the whole URL as a repo name */
    
    /* Step 2: parse the remaining path as page/ref/path */
    /* e.g., "log/main/src/file.c" → page="log", path="main/src/file.c" */
    p = strchr(url, '/');
    if (p) {
        ctx.qry.page = xstrndup(url, p - url);
        ctx.qry.path = trim_end(p + 1, '/');
    } else if (*url) {
        ctx.qry.page = xstrdup(url);
    }
}
```

## Query String Parsing: `querystring_cb()`

HTTP query parameters and POST form data are decoded by `querystring_cb()`
in `cgit.c`.  The function maps URL parameter names to `ctx.qry` fields:

```c
static void querystring_cb(const char *name, const char *value)
{
    if (!strcmp(name, "url"))      ctx.qry.url = xstrdup(value);
    else if (!strcmp(name, "p"))   ctx.qry.page = xstrdup(value);
    else if (!strcmp(name, "q"))   ctx.qry.search = xstrdup(value);
    else if (!strcmp(name, "h"))   ctx.qry.head = xstrdup(value);
    else if (!strcmp(name, "id"))  ctx.qry.sha1 = xstrdup(value);
    else if (!strcmp(name, "id2")) ctx.qry.sha2 = xstrdup(value);
    else if (!strcmp(name, "ofs")) ctx.qry.ofs = atoi(value);
    else if (!strcmp(name, "path")) ctx.qry.path = xstrdup(value);
    else if (!strcmp(name, "name")) ctx.qry.name = xstrdup(value);
    else if (!strcmp(name, "mimetype")) ctx.qry.mimetype = xstrdup(value);
    else if (!strcmp(name, "s"))   ctx.qry.sort = xstrdup(value);
    else if (!strcmp(name, "showmsg")) ctx.qry.showmsg = atoi(value);
    else if (!strcmp(name, "ss"))  ctx.qry.ssdiff = atoi(value);
    else if (!strcmp(name, "all")) ctx.qry.show_all = atoi(value);
    else if (!strcmp(name, "context")) ctx.qry.context = atoi(value);
    else if (!strcmp(name, "follow")) ctx.qry.follow = atoi(value);
    else if (!strcmp(name, "dt")) ctx.qry.dt = atoi(value);
    else if (!strcmp(name, "grep")) ctx.qry.grep = xstrdup(value);
    else if (!strcmp(name, "etag")) ctx.qry.etag = xstrdup(value);
}
```

### URL Parameter Reference

| Parameter | Query Field | Type | Description |
|-----------|------------|------|-------------|
| `url` | `qry.url` | string | Full URL path (repo/page/path) |
| `p` | `qry.page` | string | Page/command name |
| `q` | `qry.search` | string | Search string |
| `h` | `qry.head` | string | Branch/ref name |
| `id` | `qry.sha1` | string | Object SHA-1 |
| `id2` | `qry.sha2` | string | Second SHA-1 (diffs) |
| `ofs` | `qry.ofs` | int | Pagination offset |
| `path` | `qry.path` | string | File path in repo |
| `name` | `qry.name` | string | Reference/snapshot name |
| `mimetype` | `qry.mimetype` | string | MIME type override |
| `s` | `qry.sort` | string | Sort field |
| `showmsg` | `qry.showmsg` | int | Show full commit message |
| `ss` | `qry.ssdiff` | int | Side-by-side diff toggle |
| `all` | `qry.show_all` | int | Show all entries |
| `context` | `qry.context` | int | Diff context lines |
| `follow` | `qry.follow` | int | Follow renames in log |
| `dt` | `qry.dt` | int | Diff type |
| `grep` | `qry.grep` | string | Grep pattern for log search |
| `etag` | `qry.etag` | string | ETag for conditional requests |

## Command Dispatch Table

The command table in `cmd.c` maps page names to handler functions:

```c
#define def_cmd(name, want_hierarchical, want_repo, want_layout, want_vpath, is_clone) \
    {#name, cmd_##name, want_hierarchical, want_repo, want_layout, want_vpath, is_clone}

static struct cgit_cmd cmds[] = {
    def_cmd(atom,    1, 1, 0, 0, 0),
    def_cmd(about,   0, 1, 1, 0, 0),
    def_cmd(blame,   1, 1, 1, 1, 0),
    def_cmd(blob,    1, 1, 0, 0, 0),
    def_cmd(commit,  1, 1, 1, 1, 0),
    def_cmd(diff,    1, 1, 1, 1, 0),
    def_cmd(head,    1, 1, 0, 0, 1),
    def_cmd(info,    1, 1, 0, 0, 1),
    def_cmd(log,     1, 1, 1, 1, 0),
    def_cmd(ls_cache,0, 0, 0, 0, 0),
    def_cmd(objects, 1, 1, 0, 0, 1),
    def_cmd(patch,   1, 1, 1, 1, 0),
    def_cmd(plain,   1, 1, 0, 1, 0),
    def_cmd(rawdiff, 1, 1, 0, 1, 0),
    def_cmd(refs,    1, 1, 1, 0, 0),
    def_cmd(repolist,0, 0, 1, 0, 0),
    def_cmd(snapshot, 1, 1, 0, 0, 0),
    def_cmd(stats,   1, 1, 1, 1, 0),
    def_cmd(summary, 1, 1, 1, 0, 0),
    def_cmd(tag,     1, 1, 1, 0, 0),
    def_cmd(tree,    1, 1, 1, 1, 0),
};
```

### Command Flags

| Flag | Meaning |
|------|---------|
| `want_hierarchical` | Parse hierarchical path from URL |
| `want_repo` | Requires a repository context |
| `want_layout` | Render within HTML page layout |
| `want_vpath` | Accept a virtual path (file path in repo) |
| `is_clone` | HTTP clone protocol endpoint |

### Lookup: `cgit_get_cmd()`

```c
struct cgit_cmd *cgit_get_cmd(const char *name)
{
    for (int i = 0; i < ARRAY_SIZE(cmds); i++)
        if (!strcmp(cmds[i].name, name))
            return &cmds[i];
    return NULL;
}
```

The function performs a linear search.  With 21 entries, this is fast enough.

## Request Processing Flow

In `process_request()` (`cgit.c`):

```
1. Parse PATH_INFO via cgit_parse_url()
2. Parse QUERY_STRING via http_parse_querystring(querystring_cb)
3. Parse POST body (for authentication forms)
4. Resolve repository: cgit_get_repoinfo(ctx.qry.repo)
5. Determine command: cgit_get_cmd(ctx.qry.page)
6. If no page specified:
   - With repo → default to "summary"
   - Without repo → default to "repolist"
7. Check command flags:
   - want_repo but no repo → "Repository not found" error
   - is_clone and HTTP clone disabled → 404
8. Handle authentication if auth-filter is configured
9. Execute: cmd->fn(&ctx)
```

### Hierarchical Path Resolution

When `want_hierarchical=1`, cgit splits `ctx.qry.path` into a reference
(branch/tag/SHA) and a file path.  It tries progressively longer prefixes
of the path as git references until one resolves:

```
path = "main/src/lib/file.c"
try: "main"                  → found branch "main"
     qry.head = "main"
     qry.path = "src/lib/file.c"
```

If no prefix resolves, the entire path is treated as a file path within the
default branch.

## Clone Protocol Endpoints

Three commands serve the Git HTTP clone protocol:

| Endpoint | Path | Function |
|----------|------|----------|
| `info` | `repo/info/refs` | `cgit_clone_info()` — advertise refs |
| `objects` | `repo/objects/*` | `cgit_clone_objects()` — serve packfiles |
| `head` | `repo/HEAD` | `cgit_clone_head()` — serve HEAD ref |

These are only active when `enable-http-clone=1` (default).

## URL Generation

`ui-shared.c` provides URL construction helpers:

```c
const char *cgit_repourl(const char *reponame);
const char *cgit_fileurl(const char *reponame, const char *pagename,
                         const char *filename, const char *query);
const char *cgit_pageurl(const char *reponame, const char *pagename,
                         const char *query);
const char *cgit_currurl(void);
```

When `virtual-root` is set, these produce clean paths.  Otherwise, they
produce query-string URLs.

### Example URL generation:

```c
/* With virtual-root=/cgit/ */
cgit_repourl("myrepo")
    → "/cgit/myrepo/"

cgit_fileurl("myrepo", "tree", "src/main.c", "h=dev")
    → "/cgit/myrepo/tree/src/main.c?h=dev"

cgit_pageurl("myrepo", "log", "ofs=50")
    → "/cgit/myrepo/log/?ofs=50"
```

## Content-Type and HTTP Headers

The response content type is set by the command handler before generating
output.  Common types:

| Page | Content-Type |
|------|-------------|
| HTML pages | `text/html` |
| atom | `text/xml` |
| blob | auto-detected from content |
| plain | MIME type from extension or `application/octet-stream` |
| snapshot | `application/x-gzip`, etc. |
| patch | `text/plain` |
| clone endpoints | `text/plain`, `application/x-git-packed-objects` |

Headers are emitted by `cgit_print_http_headers()` in `ui-shared.c` before
any page content.

## Error Handling

If a requested repository or page is not found, cgit renders an error page
within the standard layout.  HTTP status codes:

| Condition | Status |
|-----------|--------|
| Normal page | 200 OK |
| Auth redirect | 302 Found |
| Not modified | 304 Not Modified |
| Bad request | 400 Bad Request |
| Auth required | 401 Unauthorized |
| Repo not found | 404 Not Found |
| Page not found | 404 Not Found |

The status code is set in `ctx.page.status` and emitted by the HTTP header
function.
