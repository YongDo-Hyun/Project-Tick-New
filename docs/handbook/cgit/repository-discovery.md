# cgit — Repository Discovery

## Overview

cgit discovers repositories through two mechanisms: explicit `repo.url=`
entries in the configuration file, and automatic filesystem scanning via
`scan-path`.  The scan-tree subsystem recursively searches directories for
git repositories and auto-configures them.

Source files: `scan-tree.c`, `scan-tree.h`, `shared.c` (repository list management).

## Manual Repository Configuration

Repositories can be explicitly defined in the cgitrc file:

```ini
repo.url=myproject
repo.path=/srv/git/myproject.git
repo.desc=My project description
repo.owner=Alice
```

Each `repo.url=` triggers `cgit_add_repo()` in `shared.c`, which creates a
new `cgit_repo` entry in the global repository list.

### `cgit_add_repo()`

```c
struct cgit_repo *cgit_add_repo(const char *url)
{
    struct cgit_repo *ret;
    /* grow the repo array if needed */
    if (cgit_repolist.count >= cgit_repolist.length) {
        /* realloc with doubled capacity */
    }
    ret = &cgit_repolist.repos[cgit_repolist.count++];
    /* initialize with defaults from ctx.cfg */
    ret->url = xstrdup(url);
    ret->name = ret->url;
    ret->path = NULL;
    ret->desc = cgit_default_repo_desc;
    ret->owner = NULL;
    ret->section = ctx.cfg.section;
    ret->snapshots = ctx.cfg.snapshots;
    /* ... inherit all global defaults ... */
    return ret;
}
```

## Repository Lookup

```c
struct cgit_repo *cgit_get_repoinfo(const char *url)
{
    int i;
    for (i = 0; i < cgit_repolist.count; i++) {
        if (!strcmp(cgit_repolist.repos[i].url, url))
            return &cgit_repolist.repos[i];
    }
    return NULL;
}
```

This is a linear scan — adequate for typical installations with dozens to
hundreds of repositories.

## Filesystem Scanning: `scan-path`

The `scan-path` configuration directive triggers automatic repository
discovery.  When encountered in the config file, `scan_tree()` or
`scan_projects()` is called immediately.

### `scan_tree()`

```c
void scan_tree(const char *path, repo_config_fn fn)
```

Recursively scans `path` for git repositories:

```c
static void scan_path(const char *base, const char *path, repo_config_fn fn)
{
    DIR *dir;
    struct dirent *ent;

    dir = opendir(path);
    if (!dir) return;

    while ((ent = readdir(dir)) != NULL) {
        /* skip "." and ".." */
        /* skip hidden directories unless scan-hidden-path=1 */

        if (is_git_dir(fullpath)) {
            /* found a bare repository */
            add_repo(base, fullpath, fn);
        } else if (is_git_dir(fullpath + "/.git")) {
            /* found a non-bare repository */
            add_repo(base, fullpath + "/.git", fn);
        } else {
            /* recurse into subdirectory */
            scan_path(base, fullpath, fn);
        }
    }
    closedir(dir);
}
```

### Git Directory Detection: `is_git_dir()`

```c
static int is_git_dir(const char *path)
{
    struct stat st;
    struct strbuf pathbuf = STRBUF_INIT;

    /* check for path/HEAD */
    strbuf_addf(&pathbuf, "%s/HEAD", path);
    if (stat(pathbuf.buf, &st)) {
        strbuf_release(&pathbuf);
        return 0;
    }

    /* check for path/objects */
    strbuf_reset(&pathbuf);
    strbuf_addf(&pathbuf, "%s/objects", path);
    if (stat(pathbuf.buf, &st) || !S_ISDIR(st.st_mode)) {
        strbuf_release(&pathbuf);
        return 0;
    }

    /* check for path/refs */
    strbuf_reset(&pathbuf);
    strbuf_addf(&pathbuf, "%s/refs", path);
    if (stat(pathbuf.buf, &st) || !S_ISDIR(st.st_mode)) {
        strbuf_release(&pathbuf);
        return 0;
    }

    strbuf_release(&pathbuf);
    return 1;
}
```

A directory is considered a git repository if it contains `HEAD`, `objects/`,
and `refs/` subdirectories.

### Repository Registration: `add_repo()`

When a git directory is found, `add_repo()` creates a repository entry:

```c
static void add_repo(const char *base, const char *path, repo_config_fn fn)
{
    /* derive URL from path relative to base */
    /* strip .git suffix if remove-suffix is set */
    struct cgit_repo *repo = cgit_add_repo(url);
    repo->path = xstrdup(path);

    /* read gitweb config from the repo */
    if (ctx.cfg.enable_git_config) {
        char *gitconfig = fmt("%s/config", path);
        parse_configfile(gitconfig, gitconfig_config);
    }

    /* read owner from filesystem */
    if (!repo->owner) {
        /* stat the repo dir and lookup uid owner */
        struct stat st;
        if (!stat(path, &st)) {
            struct passwd *pw = getpwuid(st.st_uid);
            if (pw)
                repo->owner = xstrdup(pw->pw_name);
        }
    }

    /* read description from description file */
    if (!repo->desc) {
        char *descfile = fmt("%s/description", path);
        /* read first line */
    }
}
```

### Git Config Integration: `gitconfig_config()`

When `enable-git-config=1`, each discovered repository's `.git/config` is
parsed for metadata:

```c
static int gitconfig_config(const char *key, const char *value)
{
    if (!strcmp(key, "gitweb.owner"))
        repo_config(repo, "owner", value);
    else if (!strcmp(key, "gitweb.description"))
        repo_config(repo, "desc", value);
    else if (!strcmp(key, "gitweb.category"))
        repo_config(repo, "section", value);
    else if (!strcmp(key, "gitweb.homepage"))
        repo_config(repo, "homepage", value);
    else if (skip_prefix(key, "cgit.", &name))
        repo_config(repo, name, value);
    return 0;
}
```

This is compatible with gitweb's configuration keys and also supports
cgit-specific `cgit.*` keys.

## Project List Scanning: `scan_projects()`

```c
void scan_projects(const char *path, const char *projectsfile,
                   repo_config_fn fn)
```

Instead of recursively scanning a directory, reads a text file listing
project paths (one per line).  Each path is appended to the base path and
checked with `is_git_dir()`.

This is useful for large installations where full recursive scanning is too
slow.

```ini
project-list=/etc/cgit/projects.list
scan-path=/srv/git
```

The `projects.list` file contains relative paths:

```
myproject.git
team/frontend.git
team/backend.git
```

## Section Derivation

When `section-from-path` is set, repository sections are automatically
derived from the directory structure:

| Value | Behavior |
|-------|----------|
| `0` | No auto-sectioning |
| `1` | First path component becomes section |
| `2` | First two components become section |
| `-1` | Last component becomes section |

Example with `section-from-path=1` and `scan-path=/srv/git`:

```
/srv/git/team/project.git     → section="team"
/srv/git/personal/test.git    → section="personal"
```

## Age File

The modification time of a repository is determined by:

1. The `agefile` (default: `info/web/last-modified`) — if this file exists
   in the repository, its contents (a date string) or modification time is
   used
2. Otherwise, the mtime of the loose `refs/` directory
3. As a fallback, the repository directory's own mtime

```c
static time_t read_agefile(const char *path)
{
    FILE *f;
    static char buf[64];

    f = fopen(path, "r");
    if (!f)
        return -1;
    if (fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return parse_date(buf, NULL);
    }
    fclose(f);
    /* fallback to file mtime */
    struct stat st;
    if (!stat(path, &st))
        return st.st_mtime;
    return 0;
}
```

## Repository List Management

The global repository list is a dynamically-sized array:

```c
struct cgit_repolist {
    int count;
    int length;          /* allocated capacity */
    struct cgit_repo *repos;
};

struct cgit_repolist cgit_repolist;
```

### Sorting

The repository list can be sorted by different criteria:

```c
static int cmp_name(const void *a, const void *b);    /* by name */
static int cmp_section(const void *a, const void *b);  /* by section */
static int cmp_idle(const void *a, const void *b);      /* by age */
```

Sorting is controlled by the `repository-sort` directive and the `s` query
parameter.

## Repository Visibility

Two directives control repository visibility:

| Directive | Effect |
|-----------|--------|
| `repo.hide=1` | Repository is hidden from the index but accessible by URL |
| `repo.ignore=1` | Repository is completely ignored |

Additionally, `strict-export` restricts export to repositories containing a
specific file (e.g., `git-daemon-export-ok`):

```ini
strict-export=git-daemon-export-ok
```

## Scan Path Caching

Scanning large directory trees can be slow.  The `cache-scanrc-ttl` directive
controls how long scan results are cached:

```ini
cache-scanrc-ttl=15   # cache scan results for 15 minutes
```

When caching is enabled, the scan is performed only when the cached result
expires.

## Configuration Reference

| Directive | Default | Description |
|-----------|---------|-------------|
| `scan-path` | (none) | Directory to scan for repos |
| `project-list` | (none) | File listing project paths |
| `enable-git-config` | 0 | Read repo metadata from git config |
| `scan-hidden-path` | 0 | Include hidden directories in scan |
| `remove-suffix` | 0 | Strip `.git` suffix from URLs |
| `section-from-path` | 0 | Auto-derive section from path |
| `strict-export` | (none) | Required file for repo visibility |
| `agefile` | `info/web/last-modified` | File checked for repo age |
| `cache-scanrc-ttl` | 15 | TTL for cached scan results (minutes) |
