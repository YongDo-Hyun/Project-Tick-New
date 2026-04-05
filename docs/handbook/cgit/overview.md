# cgit — Overview

## What Is cgit?

cgit is a fast, lightweight web frontend for Git repositories, implemented as a
CGI application written in C.  It links directly against libgit (the C library
that forms the core of the `git` command-line tool), giving it native access to
repository objects without spawning external processes for every request.  This
design makes cgit one of the fastest Git web interfaces available.

The Project Tick fork carries version `0.0.5-1-Project-Tick` (defined in the
top-level `Makefile` as `CGIT_VERSION`).  It builds against Git 2.46.0 and
extends the upstream cgit with features such as subtree display, SPDX license
detection, badge support, Code of Conduct / CLA pages, root links, and an
enhanced summary page with repository metadata.

## Key Design Goals

| Goal | How cgit achieves it |
|------|---------------------|
| **Speed** | Direct libgit linkage; file-based response cache; `sendfile()` on Linux |
| **Security** | `GIT_CONFIG_NOSYSTEM=1` set at load time; HTML entity escaping in every output function; directory-traversal guards; auth-filter framework |
| **Simplicity** | Single CGI binary; flat config file (`cgitrc`); no database requirement |
| **Extensibility** | Pluggable filter system (exec / Lua) for about, commit, source, email, owner, and auth content |

## Source File Map

The entire cgit source tree lives in `cgit/`.  Every `.c` file has a matching
`.h` (with a few exceptions such as `shared.c` and `parsing.c` which declare
their interfaces in `cgit.h`).

### Core files

| File | Purpose |
|------|---------|
| `cgit.h` | Master header — includes libgit headers; defines all major types (`cgit_repo`, `cgit_config`, `cgit_query`, `cgit_context`, etc.) and function prototypes |
| `cgit.c` | Entry point — `prepare_context()`, `config_cb()`, `querystring_cb()`, `process_request()`, `main()` |
| `shared.c` | Global variables (`cgit_repolist`, `ctx`); repo management (`cgit_add_repo`, `cgit_get_repoinfo`); diff helpers; parsing helpers |
| `parsing.c` | Commit/tag parsing (`cgit_parse_commit`, `cgit_parse_tag`, `cgit_parse_url`) |
| `cmd.c` | Command dispatch table — maps URL page names to handler functions |
| `cmd.h` | `struct cgit_cmd` definition; `cgit_get_cmd()` prototype |
| `configfile.c` | Generic `name=value` config parser (`parse_configfile`) |
| `configfile.h` | `configfile_value_fn` typedef; `parse_configfile` prototype |

### Infrastructure files

| File | Purpose |
|------|---------|
| `cache.c` / `cache.h` | File-based response cache — FNV-1 hashing, slot open/lock/fill/unlock cycle |
| `filter.c` | Filter framework — exec filters (fork/exec), Lua filters (`luaL_newstate`) |
| `html.c` / `html.h` | HTML output primitives — entity escaping, URL encoding, form helpers |
| `scan-tree.c` / `scan-tree.h` | Filesystem repository scanning — `scan_tree()`, `scan_projects()` |

### UI modules (`ui-*.c` / `ui-*.h`)

| Module | Page | Handler function |
|--------|------|-----------------|
| `ui-repolist` | `repolist` | `cgit_print_repolist()` |
| `ui-summary` | `summary` | `cgit_print_summary()` |
| `ui-log` | `log` | `cgit_print_log()` |
| `ui-commit` | `commit` | `cgit_print_commit()` |
| `ui-diff` | `diff` | `cgit_print_diff()` |
| `ui-tree` | `tree` | `cgit_print_tree()` |
| `ui-blob` | `blob` | `cgit_print_blob()` |
| `ui-refs` | `refs` | `cgit_print_refs()` |
| `ui-tag` | `tag` | `cgit_print_tag()` |
| `ui-snapshot` | `snapshot` | `cgit_print_snapshot()` |
| `ui-plain` | `plain` | `cgit_print_plain()` |
| `ui-blame` | `blame` | `cgit_print_blame()` |
| `ui-patch` | `patch` | `cgit_print_patch()` |
| `ui-atom` | `atom` | `cgit_print_atom()` |
| `ui-clone` | `HEAD` / `info` / `objects` | `cgit_clone_head()`, `cgit_clone_info()`, `cgit_clone_objects()` |
| `ui-stats` | `stats` | `cgit_show_stats()` |
| `ui-ssdiff` | (helper) | Side-by-side diff rendering via LCS algorithm |
| `ui-shared` | (helper) | HTTP headers, HTML page skeleton, link generation |

### Static assets

| File | Description |
|------|-------------|
| `cgit.css` | Default stylesheet |
| `cgit.js` | Client-side JavaScript (e.g. tree filtering) |
| `cgit.png` | Default logo |
| `favicon.ico` | Default favicon |
| `robots.txt` | Default robots file |

## Core Data Structures

All major types are defined in `cgit.h`.  The single global
`struct cgit_context ctx` (declared in `shared.c`) holds the entire request
state:

```c
struct cgit_context {
    struct cgit_environment env;   /* CGI environment variables */
    struct cgit_query       qry;   /* Parsed query/URL parameters */
    struct cgit_config      cfg;   /* Global configuration */
    struct cgit_repo       *repo;  /* Currently selected repository (or NULL) */
    struct cgit_page        page;  /* HTTP response metadata */
};
```

### `struct cgit_repo`

Represents a single Git repository.  Key fields:

```c
struct cgit_repo {
    char *url;              /* URL-visible name (e.g. "myproject") */
    char *name;             /* Display name */
    char *basename;         /* Last path component */
    char *path;             /* Filesystem path to .git directory */
    char *desc;             /* Description string */
    char *owner;            /* Repository owner */
    char *defbranch;        /* Default branch (NULL → guess from HEAD) */
    char *section;          /* Section for grouped display */
    char *clone_url;        /* Clone URL override */
    char *homepage;         /* Project homepage URL */
    struct string_list readme;   /* README file references */
    struct string_list badges;   /* Badge image URLs */
    int snapshots;          /* Bitmask of enabled snapshot formats */
    int enable_blame;       /* Whether blame view is enabled */
    int enable_commit_graph;/* Whether commit graph is shown in log */
    int enable_subtree;     /* Whether subtree detection is enabled */
    int max_stats;          /* Stats period index (0=disabled) */
    int hide;               /* 1 = hidden from listing */
    int ignore;             /* 1 = completely ignored */
    struct cgit_filter *about_filter;   /* Per-repo about filter */
    struct cgit_filter *source_filter;  /* Per-repo source highlighting */
    struct cgit_filter *email_filter;   /* Per-repo email filter */
    struct cgit_filter *commit_filter;  /* Per-repo commit message filter */
    struct cgit_filter *owner_filter;   /* Per-repo owner filter */
    /* ... */
};
```

### `struct cgit_query`

Holds all parsed URL/query-string parameters:

```c
struct cgit_query {
    int has_symref, has_oid, has_difftype;
    char *raw;          /* Raw query string */
    char *repo;         /* Repository URL */
    char *page;         /* Page name (log, commit, diff, ...) */
    char *search;       /* Search query (q=) */
    char *grep;         /* Search type (qt=) */
    char *head;         /* Branch/ref (h=) */
    char *oid, *oid2;   /* Object IDs (id=, id2=) */
    char *path;         /* Path within repository */
    char *name;         /* Snapshot filename */
    int ofs;            /* Pagination offset */
    int showmsg;        /* Show full commit messages in log */
    diff_type difftype; /* DIFF_UNIFIED / DIFF_SSDIFF / DIFF_STATONLY */
    int context;        /* Diff context lines */
    int ignorews;       /* Ignore whitespace in diffs */
    int follow;         /* Follow renames in log */
    char *vpath;        /* Virtual path (set by cmd dispatch) */
    /* ... */
};
```

## Request Lifecycle

1. **Environment setup** — The `constructor_environment()` function runs before
   `main()` (via `__attribute__((constructor))`).  It sets
   `GIT_CONFIG_NOSYSTEM=1` and `GIT_ATTR_NOSYSTEM=1`, then unsets `HOME` and
   `XDG_CONFIG_HOME` to prevent Git from reading user/system configurations.

2. **Context initialization** — `prepare_context()` zeroes out `ctx` and sets
   all configuration defaults (cache sizes, TTLs, feature flags, etc.).  CGI
   environment variables are read from `getenv()`.

3. **Configuration parsing** — `parse_configfile()` reads the cgitrc file
   (default `/etc/cgitrc`, overridable via `$CGIT_CONFIG`) and calls
   `config_cb()` for each `name=value` pair.  Repository definitions begin with
   `repo.url=` and subsequent `repo.*` directives configure that repository.

4. **Query parsing** — If running in CGI mode (no `$NO_HTTP`),
   `http_parse_querystring()` breaks the query string into name/value pairs and
   passes them to `querystring_cb()`.  The `url=` parameter is further parsed by
   `cgit_parse_url()` which splits it into repo, page, and path components.

5. **Authentication** — `authenticate_cookie()` checks whether an `auth-filter`
   is configured.  If so, it invokes the filter with function
   `"authenticate-cookie"` and sets `ctx.env.authenticated` from the filter's
   exit code.  POST requests to `/?p=login` route through
   `authenticate_post()` instead.

6. **Cache lookup** — If caching is enabled (`cache-size > 0`), a cache key is
   constructed from the URL and passed to `cache_process()`.  On a cache hit the
   stored response is sent directly via `sendfile()`.  On a miss, stdout is
   redirected to a lock file and the request proceeds through normal processing.

7. **Command dispatch** — `cgit_get_cmd()` looks up `ctx.qry.page` in the
   static `cmds[]` table (defined in `cmd.c`).  If the command requires a
   repository (`want_repo == 1`), the repository is initialized via
   `prepare_repo_env()` and `prepare_repo_cmd()`.

8. **Page rendering** — The matched command's handler function is called.  Each
   handler uses `cgit_print_http_headers()`, `cgit_print_docstart()`,
   `cgit_print_pageheader()`, and `cgit_print_docend()` (from `ui-shared.c`)
   to frame their output inside a proper HTML document.

9. **Cleanup** — `cgit_cleanup_filters()` reaps all filter resources (closing
   Lua states, freeing argv arrays).

## Version String

The version is compiled into the binary via:

```makefile
CGIT_VERSION = 0.0.5-1-Project-Tick
```

and exposed as the global:

```c
const char *cgit_version = CGIT_VERSION;
```

This string appears in the HTML footer (rendered by `ui-shared.c`) and in patch
output trailers.

## Relationship to Git

cgit is built *inside* the Git source tree.  The `Makefile` downloads
Git 2.46.0, extracts it as a `git/` subdirectory, then calls `make -C git -f
../cgit.mk` which includes Git's own `Makefile` to inherit all build variables,
object files, and linker flags.  The resulting `cgit` binary is a statically
linked combination of cgit's own object files and libgit.

## Time Constants

`cgit.h` defines convenience macros used for relative date display:

```c
#define TM_MIN    60
#define TM_HOUR  (TM_MIN * 60)
#define TM_DAY   (TM_HOUR * 24)
#define TM_WEEK  (TM_DAY * 7)
#define TM_YEAR  (TM_DAY * 365)
#define TM_MONTH (TM_YEAR / 12.0)
```

These are used by `cgit_print_age()` in `ui-shared.c` to render "2 hours ago"
style timestamps.

## Default Encoding

```c
#define PAGE_ENCODING "UTF-8"
```

All commit messages are re-encoded to UTF-8 before display (see
`cgit_parse_commit()` in `parsing.c`).

## License

cgit is licensed under the GNU General Public License v2.  The `COPYING` file
in the cgit directory contains the full text.
