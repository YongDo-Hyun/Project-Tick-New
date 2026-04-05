# cgit — Configuration Reference

## Configuration File

Default location: `/etc/cgitrc` (compiled in as `CGIT_CONFIG`).  Override at
runtime by setting the `$CGIT_CONFIG` environment variable.

## File Format

The configuration file uses a simple `name=value` format, parsed by
`parse_configfile()` in `configfile.c`.  Key rules:

- Lines starting with `#` or `;` are comments
- Leading whitespace on lines is skipped
- No quoting mechanism — the value is everything after the `=` to end of line
- Empty lines are ignored
- Nesting depth for `include=` directives is limited to 8 levels

```c
int parse_configfile(const char *filename, configfile_value_fn fn)
{
    static int nesting;
    /* ... */
    if (nesting > 8)
        return -1;
    /* ... */
    while (read_config_line(f, &name, &value))
        fn(name.buf, value.buf);
    /* ... */
}
```

## Global Directives

All global directives are processed by `config_cb()` in `cgit.c`.  When a
directive is encountered, the value is stored in the corresponding
`ctx.cfg.*` field.

### Site Identity

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `root-title` | `"Git repository browser"` | string | HTML page title for the index page |
| `root-desc` | `"a fast webinterface for the git dscm"` | string | Subtitle text on the index page |
| `root-readme` | (none) | path | Path to a file rendered on the site about page |
| `root-coc` | (none) | path | Path to Code of Conduct file |
| `root-cla` | (none) | path | Path to Contributor License Agreement file |
| `root-homepage` | (none) | URL | External homepage URL |
| `root-homepage-title` | (none) | string | Title text for the homepage link |
| `root-link` | (none) | string | `label\|url` pairs for navigation links (can repeat) |
| `logo` | `"/cgit.png"` | URL | Path to the site logo image |
| `logo-link` | (none) | URL | URL the logo links to |
| `favicon` | `"/favicon.ico"` | URL | Path to the favicon |
| `css` | (none) | URL | Stylesheet URL (can repeat for multiple stylesheets) |
| `js` | (none) | URL | JavaScript URL (can repeat) |
| `header` | (none) | path | File included at the top of every page |
| `footer` | (none) | path | File included at the bottom of every page |
| `head-include` | (none) | path | File included in the HTML `<head>` |
| `robots` | `"index, nofollow"` | string | Content for `<meta name="robots">` |

### URL Configuration

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `virtual-root` | (none) | path | Base URL path when using URL rewriting (always ends with `/`) |
| `script-name` | `CGIT_SCRIPT_NAME` | path | CGI script name (from `$SCRIPT_NAME` env var) |
| `clone-prefix` | (none) | string | Prefix for clone URLs when auto-generating |
| `clone-url` | (none) | string | Clone URL template (`$CGIT_REPO_URL` expanded) |

When `virtual-root` is set, URLs use path-based routing:
`/cgit/repo/log/path`.  Without it, query-string routing is used:
`?url=repo/log/path`.

### Feature Flags

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `enable-http-clone` | `1` | int | Allow HTTP clone operations (HEAD, info/refs, objects/) |
| `enable-index-links` | `0` | int | Show log/tree/commit links on the repo index page |
| `enable-index-owner` | `1` | int | Show the Owner column on the repo index page |
| `enable-blame` | `0` | int | Enable blame view for all repos |
| `enable-commit-graph` | `0` | int | Show ASCII commit graph in log view |
| `enable-log-filecount` | `0` | int | Show changed-file count in log view |
| `enable-log-linecount` | `0` | int | Show added/removed line counts in log |
| `enable-remote-branches` | `0` | int | Display remote tracking branches |
| `enable-subject-links` | `0` | int | Show parent commit subjects instead of hashes |
| `enable-html-serving` | `0` | int | Serve HTML files as-is from plain view |
| `enable-subtree` | `0` | int | Detect and display git-subtree directories |
| `enable-tree-linenumbers` | `1` | int | Show line numbers in file/blob view |
| `enable-git-config` | `0` | int | Read `gitweb.*` and `cgit.*` from repo's git config |
| `enable-filter-overrides` | `0` | int | Allow repos to override global filters |
| `enable-follow-links` | `0` | int | Show "follow" links in log view for renames |
| `embedded` | `0` | int | Omit HTML boilerplate for embedding in another page |
| `noheader` | `0` | int | Suppress the page header |
| `noplainemail` | `0` | int | Hide email addresses in output |
| `local-time` | `0` | int | Display times in local timezone instead of UTC |

### Limits

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `max-repo-count` | `50` | int | Repos per page on the index (≤0 → unlimited) |
| `max-commit-count` | `50` | int | Commits per page in log view |
| `max-message-length` | `80` | int | Truncate commit subject at this length |
| `max-repodesc-length` | `80` | int | Truncate repo description at this length |
| `max-blob-size` | `0` | int (KB) | Max blob size to display (0 = unlimited) |
| `max-stats` | `0` | int | Stats period (0=disabled, 1=week, 2=month, 3=quarter, 4=year) |
| `max-atom-items` | `10` | int | Number of entries in Atom feeds |
| `max-subtree-commits` | `2000` | int | Max commits to scan for subtree trailers |
| `renamelimit` | `-1` | int | Diff rename detection limit (-1 = Git default) |

### Caching

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `cache-size` | `0` | int | Number of cache entries (0 = disabled) |
| `cache-root` | `CGIT_CACHE_ROOT` | path | Directory for cache files |
| `cache-root-ttl` | `5` | int (min) | TTL for repo-list pages |
| `cache-repo-ttl` | `5` | int (min) | TTL for repo-specific pages |
| `cache-dynamic-ttl` | `5` | int (min) | TTL for dynamic content |
| `cache-static-ttl` | `-1` | int (min) | TTL for static content (-1 = forever) |
| `cache-about-ttl` | `15` | int (min) | TTL for about/readme pages |
| `cache-snapshot-ttl` | `5` | int (min) | TTL for snapshot pages |
| `cache-scanrc-ttl` | `15` | int (min) | TTL for cached scan-path results |

### Sorting

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `case-sensitive-sort` | `1` | int | Case-sensitive repo name sorting |
| `section-sort` | `1` | int | Sort sections alphabetically |
| `section-from-path` | `0` | int | Derive section name from path depth (>0 = from start, <0 = from end) |
| `repository-sort` | `"name"` | string | Default sort field for repo list |
| `branch-sort` | `0` | int | Branch sort: 0=name, 1=age |
| `commit-sort` | `0` | int | Commit sort: 0=default, 1=date, 2=topo |

### Snapshots

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `snapshots` | (none) | string | Space-separated list of enabled formats: `.tar` `.tar.gz` `.tar.bz2` `.tar.lz` `.tar.xz` `.tar.zst` `.zip`.  Also accepts `all`. |

### Filters

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `about-filter` | (none) | filter | Filter for rendering README/about content |
| `source-filter` | (none) | filter | Filter for syntax highlighting source code |
| `commit-filter` | (none) | filter | Filter for commit messages |
| `email-filter` | (none) | filter | Filter for email display (2 args: email, page) |
| `owner-filter` | (none) | filter | Filter for owner display |
| `auth-filter` | (none) | filter | Authentication filter (12 args) |

Filter values use the format `type:command`:
- `exec:/path/to/script` — external process filter
- `lua:/path/to/script.lua` — Lua script filter
- Plain path without prefix defaults to `exec`

### Display

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `summary-branches` | `10` | int | Branches shown on summary page |
| `summary-tags` | `10` | int | Tags shown on summary page |
| `summary-log` | `10` | int | Log entries shown on summary page |
| `side-by-side-diffs` | `0` | int | Default to side-by-side diff view |
| `remove-suffix` | `0` | int | Remove `.git` suffix from repo URLs |
| `scan-hidden-path` | `0` | int | Include hidden dirs when scanning |

### Miscellaneous

| Directive | Default | Type | Description |
|-----------|---------|------|-------------|
| `agefile` | `"info/web/last-modified"` | path | File in repo checked for modification time |
| `mimetype-file` | (none) | path | Apache-style mime.types file |
| `mimetype.<ext>` | (none) | string | MIME type for a file extension |
| `module-link` | (none) | URL | URL template for submodule links |
| `strict-export` | (none) | path | Only export repos containing this file |
| `project-list` | (none) | path | File listing project directories for `scan-path` |
| `scan-path` | (none) | path | Directory to scan for git repositories |
| `readme` | (none) | string | Default README file spec (can repeat) |
| `include` | (none) | path | Include another config file |

## Repository Directives

Repository configuration begins with `repo.url=` which creates a new
repository entry via `cgit_add_repo()`.  Subsequent `repo.*` directives
modify the most recently created repository via `repo_config()` in `cgit.c`.

| Directive | Description |
|-----------|-------------|
| `repo.url` | Repository URL path (triggers new repo creation) |
| `repo.path` | Filesystem path to the git repository |
| `repo.name` | Display name |
| `repo.basename` | Override for basename derivation |
| `repo.desc` | Repository description |
| `repo.owner` | Repository owner name |
| `repo.homepage` | Project homepage URL |
| `repo.defbranch` | Default branch name |
| `repo.section` | Section heading for grouped display |
| `repo.clone-url` | Clone URL (overrides global) |
| `repo.readme` | README file spec (`[ref:]path`, can repeat) |
| `repo.logo` | Per-repo logo URL |
| `repo.logo-link` | Per-repo logo link URL |
| `repo.extra-head-content` | Extra HTML for `<head>` |
| `repo.snapshots` | Snapshot format mask (space-separated suffixes) |
| `repo.snapshot-prefix` | Prefix for snapshot filenames |
| `repo.enable-blame` | Override global enable-blame |
| `repo.enable-commit-graph` | Override global enable-commit-graph |
| `repo.enable-log-filecount` | Override global enable-log-filecount |
| `repo.enable-log-linecount` | Override global enable-log-linecount |
| `repo.enable-remote-branches` | Override global enable-remote-branches |
| `repo.enable-subject-links` | Override global enable-subject-links |
| `repo.enable-html-serving` | Override global enable-html-serving |
| `repo.enable-subtree` | Override global enable-subtree |
| `repo.max-stats` | Override global max-stats |
| `repo.max-subtree-commits` | Override global max-subtree-commits |
| `repo.branch-sort` | `"age"` or `"name"` |
| `repo.commit-sort` | `"date"` or `"topo"` |
| `repo.module-link` | Submodule URL template |
| `repo.module-link.<submodule>` | Per-submodule URL |
| `repo.badge` | Badge entry: `url\|imgurl` or just `imgurl` (can repeat) |
| `repo.hide` | `1` = hide from listing (still accessible by URL) |
| `repo.ignore` | `1` = completely ignore this repository |

### Filter overrides (require `enable-filter-overrides=1`)

| Directive | Description |
|-----------|-------------|
| `repo.about-filter` | Per-repo about filter |
| `repo.commit-filter` | Per-repo commit filter |
| `repo.source-filter` | Per-repo source filter |
| `repo.email-filter` | Per-repo email filter |
| `repo.owner-filter` | Per-repo owner filter |

## Repository Defaults

When a new repository is created by `cgit_add_repo()`, it inherits all global
defaults from `ctx.cfg`:

```c
ret->section = ctx.cfg.section;
ret->snapshots = ctx.cfg.snapshots;
ret->enable_blame = ctx.cfg.enable_blame;
ret->enable_commit_graph = ctx.cfg.enable_commit_graph;
ret->enable_log_filecount = ctx.cfg.enable_log_filecount;
ret->enable_log_linecount = ctx.cfg.enable_log_linecount;
ret->enable_remote_branches = ctx.cfg.enable_remote_branches;
ret->enable_subject_links = ctx.cfg.enable_subject_links;
ret->enable_html_serving = ctx.cfg.enable_html_serving;
ret->enable_subtree = ctx.cfg.enable_subtree;
ret->max_stats = ctx.cfg.max_stats;
ret->max_subtree_commits = ctx.cfg.max_subtree_commits;
ret->branch_sort = ctx.cfg.branch_sort;
ret->commit_sort = ctx.cfg.commit_sort;
ret->module_link = ctx.cfg.module_link;
ret->readme = ctx.cfg.readme;
ret->about_filter = ctx.cfg.about_filter;
ret->commit_filter = ctx.cfg.commit_filter;
ret->source_filter = ctx.cfg.source_filter;
ret->email_filter = ctx.cfg.email_filter;
ret->owner_filter = ctx.cfg.owner_filter;
ret->clone_url = ctx.cfg.clone_url;
```

This means global directives should appear *before* `repo.url=` entries, since
they set the defaults for subsequently defined repositories.

## Git Config Integration

When `enable-git-config=1`, the `scan-tree` scanner reads each repository's
`.git/config` and maps gitweb-compatible directives:

```c
if (!strcmp(key, "gitweb.owner"))
    config_fn(repo, "owner", value);
else if (!strcmp(key, "gitweb.description"))
    config_fn(repo, "desc", value);
else if (!strcmp(key, "gitweb.category"))
    config_fn(repo, "section", value);
else if (!strcmp(key, "gitweb.homepage"))
    config_fn(repo, "homepage", value);
else if (skip_prefix(key, "cgit.", &name))
    config_fn(repo, name, value);
```

Any `cgit.*` key in the git config is passed directly to the repo config
handler, allowing per-repo settings without modifying the global cgitrc.

## README File Spec Format

README directives support three forms:

| Format | Meaning |
|--------|---------|
| `path` | File on disk, relative to repo path |
| `/absolute/path` | File on disk, absolute |
| `ref:path` | File tracked in the git repository at the given ref |
| `:path` | File tracked in the default branch or query head |

Multiple `readme` directives can be specified.  cgit tries each in order and
uses the first one found (checked via `cgit_ref_path_exists()` for tracked
files, or `access(R_OK)` for disk files).

## Macro Expansion

The `expand_macros()` function (in `shared.c`) performs environment variable
substitution in certain directive values (`cache-root`, `scan-path`,
`project-list`, `include`).  A `$VARNAME` or `${VARNAME}` in the value is
replaced with the corresponding environment variable.

## Example Configuration

```ini
# Site settings
root-title=Project Tick Git
root-desc=Source code for Project Tick
logo=/cgit/cgit.png
css=/cgit/cgit.css
virtual-root=/cgit/

# Features
enable-commit-graph=1
enable-blame=1
enable-http-clone=1
enable-index-links=1
snapshots=tar.gz tar.xz zip
max-stats=quarter

# Caching
cache-size=1000
cache-root=/var/cache/cgit

# Filters
source-filter=exec:/usr/lib/cgit/filters/syntax-highlighting.py
about-filter=exec:/usr/lib/cgit/filters/about-formatting.sh

# Scanning
scan-path=/srv/git/
section-from-path=1

# Or manual repo definitions:
repo.url=myproject
repo.path=/srv/git/myproject.git
repo.desc=My awesome project
repo.owner=Alice
repo.readme=master:README.md
repo.clone-url=https://git.example.com/myproject.git
repo.snapshots=tar.gz zip
repo.badge=https://ci.example.com/badge.svg|https://ci.example.com/
```
