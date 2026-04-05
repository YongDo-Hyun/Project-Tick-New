# cgit — API Reference

## Overview

This document catalogs all public function prototypes, types, and global
variables exported by cgit's header files.  Functions are grouped by header
file and module.

## `cgit.h` — Core Types and Functions

### Core Structures

```c
struct cgit_environment {
    const char *cgit_config;     /* CGIT_CONFIG env var */
    const char *http_host;       /* HTTP_HOST */
    const char *https;           /* HTTPS */
    const char *no_http;         /* NO_HTTP */
    const char *http_cookie;     /* HTTP_COOKIE */
    const char *request_method;  /* REQUEST_METHOD */
    const char *query_string;    /* QUERY_STRING */
    const char *http_referer;    /* HTTP_REFERER */
    const char *path_info;       /* PATH_INFO */
    const char *script_name;     /* SCRIPT_NAME */
    const char *server_name;     /* SERVER_NAME */
    const char *server_port;     /* SERVER_PORT */
    const char *http_accept;     /* HTTP_ACCEPT */
    int authenticated;           /* authentication result */
};

struct cgit_query {
    char *raw;
    char *repo;
    char *page;
    char *search;
    char *grep;
    char *head;
    char *sha1;
    char *sha2;
    char *path;
    char *name;
    char *url;
    char *mimetype;
    char *etag;
    int nohead;
    int ofs;
    int has_symref;
    int has_sha1;
    int has_dot;
    int ignored;
    char *sort;
    int showmsg;
    int ssdiff;
    int show_all;
    int context;
    int follow;
    int dt;
    int log_hierarchical_threading;
};

struct cgit_page {
    const char *mimetype;
    const char *charset;
    const char *filename;
    const char *etag;
    const char *title;
    int status;
    time_t modified;
    time_t expires;
    size_t size;
};

struct cgit_config {
    char *root_title;
    char *root_desc;
    char *root_readme;
    char *root_coc;
    char *root_cla;
    char *root_homepage;
    char *root_homepage_title;
    struct string_list root_links;
    char *css;
    struct string_list css_list;
    char *js;
    struct string_list js_list;
    char *logo;
    char *logo_link;
    char *favicon;
    char *header;
    char *footer;
    char *head_include;
    char *module_link;
    char *virtual_root;
    char *script_name;
    char *section;
    char *cache_root;
    char *robots;
    char *clone_prefix;
    char *clone_url;
    char *readme;
    char *agefile;
    char *project_list;
    char *strict_export;
    char *mimetype_file;
    /* ... filter pointers, integer flags, limits ... */
    int cache_size;
    int cache_root_ttl;
    int cache_repo_ttl;
    int cache_dynamic_ttl;
    int cache_static_ttl;
    int cache_about_ttl;
    int cache_snapshot_ttl;
    int cache_scanrc_ttl;
    int max_repo_count;
    int max_commit_count;
    int max_message_length;
    int max_repodesc_length;
    int max_blob_size;
    int max_stats;
    int max_atom_items;
    int max_subtree_commits;
    int summary_branches;
    int summary_tags;
    int summary_log;
    int snapshots;
    int enable_http_clone;
    int enable_index_links;
    int enable_index_owner;
    int enable_blame;
    int enable_commit_graph;
    int enable_log_filecount;
    int enable_log_linecount;
    int enable_remote_branches;
    int enable_subject_links;
    int enable_html_serving;
    int enable_subtree;
    int enable_tree_linenumbers;
    int enable_git_config;
    int enable_filter_overrides;
    int enable_follow_links;
    int embedded;
    int noheader;
    int noplainemail;
    int local_time;
    int case_sensitive_sort;
    int section_sort;
    int section_from_path;
    int side_by_side_diffs;
    int remove_suffix;
    int scan_hidden_path;
    int branch_sort;
    int commit_sort;
    int renamelimit;
};

struct cgit_repo {
    char *url;
    char *name;
    char *basename;
    char *path;
    char *desc;
    char *owner;
    char *homepage;
    char *defbranch;
    char *section;
    char *clone_url;
    char *logo;
    char *logo_link;
    char *readme;
    char *module_link;
    char *extra_head_content;
    char *snapshot_prefix;
    struct string_list badges;
    struct cgit_filter *about_filter;
    struct cgit_filter *commit_filter;
    struct cgit_filter *source_filter;
    struct cgit_filter *email_filter;
    struct cgit_filter *owner_filter;
    int snapshots;
    int enable_blame;
    int enable_commit_graph;
    int enable_log_filecount;
    int enable_log_linecount;
    int enable_remote_branches;
    int enable_subject_links;
    int enable_html_serving;
    int enable_subtree;
    int max_stats;
    int max_subtree_commits;
    int branch_sort;
    int commit_sort;
    int hide;
    int ignore;
};

struct cgit_context {
    struct cgit_environment env;
    struct cgit_query qry;
    struct cgit_config cfg;
    struct cgit_page page;
    struct cgit_repo *repo;
};
```

### Global Variables

```c
extern struct cgit_context ctx;
extern struct cgit_repolist cgit_repolist;
extern const char *cgit_version;
```

### Repository Management

```c
extern struct cgit_repo *cgit_add_repo(const char *url);
extern struct cgit_repo *cgit_get_repoinfo(const char *url);
```

### Parsing Functions

```c
extern void cgit_parse_url(const char *url);
extern struct commitinfo *cgit_parse_commit(struct commit *commit);
extern struct taginfo *cgit_parse_tag(struct tag *tag);
extern void cgit_free_commitinfo(struct commitinfo *info);
extern void cgit_free_taginfo(struct taginfo *info);
```

### Diff Functions

```c
typedef void (*filepair_fn)(struct diff_filepair *pair);
typedef void (*linediff_fn)(char *line, int len);

extern void cgit_diff_tree(const struct object_id *old_oid,
                           const struct object_id *new_oid,
                           filepair_fn fn, const char *prefix,
                           int renamelimit);
extern void cgit_diff_commit(struct commit *commit, filepair_fn fn,
                             const char *prefix);
extern void cgit_diff_files(const struct object_id *old_oid,
                            const struct object_id *new_oid,
                            unsigned long *old_size,
                            unsigned long *new_size,
                            int *binary, int context,
                            int ignorews, linediff_fn fn);
```

### Snapshot Functions

```c
extern int cgit_parse_snapshots_mask(const char *str);

extern const struct cgit_snapshot_format cgit_snapshot_formats[];
```

### Filter Functions

```c
extern struct cgit_filter *cgit_new_filter(const char *cmd, filter_type type);
extern int cgit_open_filter(struct cgit_filter *filter, ...);
extern int cgit_close_filter(struct cgit_filter *filter);
```

### Utility Functions

```c
extern const char *cgit_repobasename(const char *reponame);
extern char *cgit_default_repo_desc;
extern int cgit_ref_path_exists(const char *path, const char *ref, int file_only);
```

## `html.h` — HTML Output Functions

```c
extern const char *fmt(const char *format, ...);
extern char *fmtalloc(const char *format, ...);

extern void html_raw(const char *data, size_t size);
extern void html(const char *txt);
extern void htmlf(const char *format, ...);
extern void html_txt(const char *txt);
extern void html_ntxt(const char *txt, int len);
extern void html_attr(const char *txt);
extern void html_url_path(const char *txt);
extern void html_url_arg(const char *txt);
extern void html_hidden(const char *name, const char *value);
extern void html_option(const char *value, const char *text,
                        const char *selected_value);
extern void html_link_open(const char *url, const char *title,
                           const char *class);
extern void html_link_close(void);
extern void html_include(const char *filename);
extern void html_checkbox(const char *name, int value);
extern void html_txt_input(const char *name, const char *value, int size);
```

## `ui-shared.h` — Page Layout and Links

### HTTP and Layout

```c
extern void cgit_print_http_headers(void);
extern void cgit_print_docstart(void);
extern void cgit_print_docend(void);
extern void cgit_print_pageheader(void);
extern void cgit_print_layout_start(void);
extern void cgit_print_layout_end(void);
extern void cgit_print_error(const char *msg);
extern void cgit_print_error_page(int code, const char *msg,
                                  const char *fmt, ...);
```

### URL Generation

```c
extern const char *cgit_repourl(const char *reponame);
extern const char *cgit_fileurl(const char *reponame, const char *pagename,
                                const char *filename, const char *query);
extern const char *cgit_pageurl(const char *reponame, const char *pagename,
                                const char *query);
extern const char *cgit_currurl(void);
extern const char *cgit_rooturl(void);
```

### Link Functions

```c
extern void cgit_summary_link(const char *name, const char *title,
                              const char *class, const char *head);
extern void cgit_tag_link(const char *name, const char *title,
                          const char *class, const char *tag);
extern void cgit_tree_link(const char *name, const char *title,
                           const char *class, const char *head,
                           const char *rev, const char *path);
extern void cgit_log_link(const char *name, const char *title,
                          const char *class, const char *head,
                          const char *rev, const char *path,
                          int ofs, const char *grep, const char *pattern,
                          int showmsg, int follow);
extern void cgit_commit_link(const char *name, const char *title,
                             const char *class, const char *head,
                             const char *rev, const char *path);
extern void cgit_patch_link(const char *name, const char *title,
                            const char *class, const char *head,
                            const char *rev, const char *path);
extern void cgit_refs_link(const char *name, const char *title,
                           const char *class, const char *head,
                           const char *rev, const char *path);
extern void cgit_diff_link(const char *name, const char *title,
                           const char *class, const char *head,
                           const char *new_rev, const char *old_rev,
                           const char *path, int toggle);
extern void cgit_stats_link(const char *name, const char *title,
                            const char *class, const char *head,
                            const char *path);
extern void cgit_plain_link(const char *name, const char *title,
                            const char *class, const char *head,
                            const char *rev, const char *path);
extern void cgit_blame_link(const char *name, const char *title,
                            const char *class, const char *head,
                            const char *rev, const char *path);
extern void cgit_object_link(struct object *obj);
extern void cgit_submodule_link(const char *name, const char *path,
                                const char *commit);
extern void cgit_print_snapshot_links(const char *repo, const char *head,
                                      const char *hex, int snapshots);
extern void cgit_print_branches(int max);
extern void cgit_print_tags(int max);
```

### Diff Helpers

```c
extern void cgit_print_diff_hunk_header(int oldofs, int oldcnt,
                                         int newofs, int newcnt,
                                         const char *func);
extern void cgit_print_diff_line_prefix(int type);
```

## `cmd.h` — Command Dispatch

```c
struct cgit_cmd {
    const char *name;
    void (*fn)(struct cgit_context *ctx);
    unsigned int want_hierarchical:1;
    unsigned int want_repo:1;
    unsigned int want_layout:1;
    unsigned int want_vpath:1;
    unsigned int is_clone:1;
};

extern struct cgit_cmd *cgit_get_cmd(const char *name);
```

## `cache.h` — Cache System

```c
typedef void (*cache_fill_fn)(void *cbdata);

extern int cache_process(int size, const char *path, const char *key,
                         int ttl, cache_fill_fn fn, void *cbdata);
extern int cache_ls(const char *path);
extern unsigned long hash_str(const char *str);
```

## `configfile.h` — Configuration File Parser

```c
typedef void (*configfile_value_fn)(const char *name, const char *value);

extern int parse_configfile(const char *filename, configfile_value_fn fn);
```

## `scan-tree.h` — Repository Scanner

```c
typedef void (*repo_config_fn)(struct cgit_repo *repo,
                               const char *name, const char *value);

extern void scan_projects(const char *path, const char *projectsfile,
                          repo_config_fn fn);
extern void scan_tree(const char *path, repo_config_fn fn);
```

## `filter.c` — Filter Types

```c
#define ABOUT_FILTER   0
#define COMMIT_FILTER  1
#define SOURCE_FILTER  2
#define EMAIL_FILTER   3
#define AUTH_FILTER    4
#define OWNER_FILTER   5

typedef int filter_type;
```

## UI Module Entry Points

Each `ui-*.c` module exposes one or more public functions:

| Module | Function | Description |
|--------|----------|-------------|
| `ui-atom.c` | `cgit_print_atom(char *tip, char *path, int max)` | Generate Atom feed |
| `ui-blame.c` | `cgit_print_blame(void)` | Render blame view |
| `ui-blob.c` | `cgit_print_blob(const char *hex, char *path, const char *head, int file_only)` | Display blob content |
| `ui-clone.c` | `cgit_clone_info(void)` | HTTP clone: `info/refs` |
| `ui-clone.c` | `cgit_clone_objects(void)` | HTTP clone: pack objects |
| `ui-clone.c` | `cgit_clone_head(void)` | HTTP clone: `HEAD` ref |
| `ui-commit.c` | `cgit_print_commit(const char *rev, const char *prefix)` | Display commit |
| `ui-diff.c` | `cgit_print_diff(const char *new_rev, const char *old_rev, const char *prefix, int show_ctrls, int raw)` | Render diff |
| `ui-diff.c` | `cgit_print_diffstat(const struct object_id *old, const struct object_id *new, const char *prefix)` | Render diffstat |
| `ui-log.c` | `cgit_print_log(const char *tip, int ofs, int cnt, char *grep, char *pattern, char *path, int pager, int commit_graph, int commit_sort)` | Display log |
| `ui-patch.c` | `cgit_print_patch(const char *new_rev, const char *old_rev, const char *prefix)` | Generate patch |
| `ui-plain.c` | `cgit_print_plain(void)` | Serve raw file content |
| `ui-refs.c` | `cgit_print_refs(void)` | Display branches and tags |
| `ui-repolist.c` | `cgit_print_repolist(void)` | Repository index page |
| `ui-snapshot.c` | `cgit_print_snapshot(const char *head, const char *hex, const char *prefix, const char *filename, int snapshots)` | Generate archive |
| `ui-stats.c` | `cgit_print_stats(void)` | Display statistics |
| `ui-summary.c` | `cgit_print_summary(void)` | Repository summary page |
| `ui-ssdiff.c` | `cgit_ssdiff_header_begin(void)` | Start ssdiff output |
| `ui-ssdiff.c` | `cgit_ssdiff_header_end(void)` | End ssdiff header |
| `ui-ssdiff.c` | `cgit_ssdiff_footer(void)` | End ssdiff output |
| `ui-tag.c` | `cgit_print_tag(const char *revname)` | Display tag |
| `ui-tree.c` | `cgit_print_tree(const char *rev, char *path)` | Display tree |
