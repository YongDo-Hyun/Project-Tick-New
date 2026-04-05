# cgit — Filter System

## Overview

cgit provides a pluggable content filtering pipeline that transforms text
before it is rendered in HTML output.  Filters are used for tasks such as
syntax highlighting, README rendering, email obfuscation, and authentication.

Source file: `filter.c`.

## Filter Types

Six filter types are defined, each identified by a constant and linked to an
entry in the `filter_specs[]` table:

```c
#define ABOUT_FILTER   0   /* README/about page rendering */
#define COMMIT_FILTER  1   /* commit message formatting */
#define SOURCE_FILTER  2   /* source code syntax highlighting */
#define EMAIL_FILTER   3   /* email address display */
#define AUTH_FILTER    4   /* authentication/authorization */
#define OWNER_FILTER   5   /* owner field display */
```

### Filter Specs Table

```c
static struct {
    char *prefix;
    int args;
} filter_specs[] = {
    [ABOUT_FILTER]  = { "about",  1 },
    [COMMIT_FILTER] = { "commit", 0 },
    [SOURCE_FILTER] = { "source", 1 },
    [EMAIL_FILTER]  = { "email",  2 },  /* email, page */
    [AUTH_FILTER]    = { "auth",  12 },
    [OWNER_FILTER]  = { "owner",  0 },
};
```

The `args` field specifies the number of *extra* arguments the filter
receives (beyond the filter command itself).

## Filter Structure

```c
struct cgit_filter {
    char *cmd;               /* command or script path */
    int type;                /* filter type constant */
    int (*open)(struct cgit_filter *, ...);   /* start filter */
    int (*close)(struct cgit_filter *);       /* finish filter */
    void (*fprintf)(struct cgit_filter *, FILE *, const char *fmt, ...);
    void (*cleanup)(struct cgit_filter *);    /* free resources */
    int argument_count;      /* from filter_specs */
};
```

Two implementations exist:

| Implementation | Struct | Description |
|---------------|--------|-------------|
| Exec filter | `struct cgit_exec_filter` | Fork/exec an external process |
| Lua filter | `struct cgit_lua_filter` | Execute a Lua script in-process |

## Exec Filters

Exec filters fork a child process and redirect `stdout` through a pipe.  All
data written to `stdout` while the filter is open passes through the child
process, which can transform it before output.

### Structure

```c
struct cgit_exec_filter {
    struct cgit_filter base;
    char *cmd;
    char **argv;
    int old_stdout;   /* saved fd for restoring stdout */
    int pipe_fh[2];   /* pipe: [read, write] */
    pid_t pid;        /* child process id */
};
```

### Open Phase

```c
static int open_exec_filter(struct cgit_filter *base, ...)
{
    struct cgit_exec_filter *f = (struct cgit_exec_filter *)base;
    /* create pipe */
    pipe(f->pipe_fh);
    /* save stdout */
    f->old_stdout = dup(STDOUT_FILENO);
    /* fork */
    f->pid = fork();
    if (f->pid == 0) {
        /* child: redirect stdin from pipe read end */
        dup2(f->pipe_fh[0], STDIN_FILENO);
        close(f->pipe_fh[0]);
        close(f->pipe_fh[1]);
        /* exec the filter command with extra args from va_list */
        execvp(f->cmd, f->argv);
        /* on failure: */
        exit(1);
    }
    /* parent: redirect stdout to pipe write end */
    dup2(f->pipe_fh[1], STDOUT_FILENO);
    close(f->pipe_fh[0]);
    close(f->pipe_fh[1]);
    return 0;
}
```

### Close Phase

```c
static int close_exec_filter(struct cgit_filter *base)
{
    struct cgit_exec_filter *f = (struct cgit_exec_filter *)base;
    int status;
    fflush(stdout);
    /* restore original stdout */
    dup2(f->old_stdout, STDOUT_FILENO);
    close(f->old_stdout);
    /* wait for child */
    waitpid(f->pid, &status, 0);
    /* return child exit status */
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}
```

### Argument Passing

Extra arguments (from `filter_specs[].args`) are passed via `va_list` in the
open function and become `argv` entries for the child process:

| Filter Type | argv[0] | argv[1] | argv[2] | ... |
|-------------|---------|---------|---------|-----|
| ABOUT | cmd | filename | — | — |
| SOURCE | cmd | filename | — | — |
| COMMIT | cmd | — | — | — |
| OWNER | cmd | — | — | — |
| EMAIL | cmd | email | page | — |
| AUTH | cmd | (12 args: method, mimetype, http_host, https, authenticated, username, http_cookie, request_method, query_string, referer, path, http_accept) |

## Lua Filters

When cgit is compiled with Lua support, filters can be Lua scripts executed
in-process without fork/exec overhead.

### Structure

```c
struct cgit_lua_filter {
    struct cgit_filter base;
    char *script_file;
    lua_State *lua_state;
};
```

### Lua API

The Lua script must define a `filter_open()` and `filter_close()` function.
Data is passed to the Lua script through a custom `write()` function
registered in the Lua environment.

```lua
-- Example source filter
function filter_open(filename)
    -- Called when the filter opens
    -- filename is the file being processed
end

function write(str)
    -- Called with chunks of content to filter
    -- Write transformed output
    html(str)
end

function filter_close()
    -- Called when filtering is complete
    return 0  -- return exit code
end
```

### Lua C Bindings

cgit registers several C functions into the Lua environment:

```c
lua_pushcfunction(lua_state, lua_html);       /* html() */
lua_pushcfunction(lua_state, lua_html_txt);   /* html_txt() */
lua_pushcfunction(lua_state, lua_html_attr);  /* html_attr() */
lua_pushcfunction(lua_state, lua_html_url_path); /* html_url_path() */
lua_pushcfunction(lua_state, lua_html_url_arg);  /* html_url_arg() */
lua_pushcfunction(lua_state, lua_html_include);  /* include() */
```

These correspond to the C functions in `html.c` and allow the Lua script to
produce properly escaped HTML output.

### Lua Filter Open

```c
static int open_lua_filter(struct cgit_filter *base, ...)
{
    struct cgit_lua_filter *f = (struct cgit_lua_filter *)base;
    /* Load and execute the Lua script if not already loaded */
    if (!f->lua_state) {
        f->lua_state = luaL_newstate();
        luaL_openlibs(f->lua_state);
        /* register C bindings */
        /* load script file */
    }
    /* redirect write() calls to the Lua state */
    /* call filter_open() in the Lua script, passing extra args */
    return 0;
}
```

### Lua Filter Close

```c
static int close_lua_filter(struct cgit_filter *base)
{
    struct cgit_lua_filter *f = (struct cgit_lua_filter *)base;
    /* call filter_close() in the Lua script */
    /* return the script's exit code */
    return lua_tointeger(f->lua_state, -1);
}
```

## Filter Construction

`cgit_new_filter()` creates a new filter instance:

```c
struct cgit_filter *cgit_new_filter(const char *cmd, filter_type type)
{
    if (!cmd || !*cmd)
        return NULL;

    if (!prefixcmp(cmd, "lua:")) {
        /* create Lua filter */
        return new_lua_filter(cmd + 4, type);
    }
    if (!prefixcmp(cmd, "exec:")) {
        /* create exec filter, stripping prefix */
        return new_exec_filter(cmd + 5, type);
    }
    /* default: treat as exec filter */
    return new_exec_filter(cmd, type);
}
```

Prefix rules:
- `lua:/path/to/script.lua` → Lua filter
- `exec:/path/to/script` → exec filter
- `/path/to/script` (no prefix) → exec filter (backward compatibility)

## Filter Usage Points

### About Filter (`ABOUT_FILTER`)

Applied when rendering README and about pages.  Called from `ui-summary.c`
and the about view:

```c
cgit_open_filter(ctx.repo->about_filter, filename);
/* write README content */
cgit_close_filter(ctx.repo->about_filter);
```

Common use: converting Markdown to HTML.

### Source Filter (`SOURCE_FILTER`)

Applied when displaying file contents in blob/tree views.  Called from
`ui-tree.c`:

```c
cgit_open_filter(ctx.repo->source_filter, filename);
/* write file content */
cgit_close_filter(ctx.repo->source_filter);
```

Common use: syntax highlighting.

### Commit Filter (`COMMIT_FILTER`)

Applied to commit messages in log and commit views.  Called from `ui-log.c`
and `ui-commit.c`:

```c
cgit_open_filter(ctx.repo->commit_filter);
html_txt(info->msg);
cgit_close_filter(ctx.repo->commit_filter);
```

Common use: linkifying issue references.

### Email Filter (`EMAIL_FILTER`)

Applied to author/committer email addresses.  Receives the email address and
current page name as arguments:

```c
cgit_open_filter(ctx.repo->email_filter, email, page);
html_txt(email);
cgit_close_filter(ctx.repo->email_filter);
```

Common use: gravatar integration, email obfuscation.

### Auth Filter (`AUTH_FILTER`)

Used for cookie-based authentication.  Receives 12 arguments covering the
full HTTP request context.  See `authentication.md` for details.

### Owner Filter (`OWNER_FILTER`)

Applied when displaying the repository owner.

## Shipped Filter Scripts

cgit ships with filter scripts in the `filters/` directory:

| Script | Type | Description |
|--------|------|-------------|
| `syntax-highlighting.py` | SOURCE | Python-based syntax highlighter using Pygments |
| `syntax-highlighting.sh` | SOURCE | Shell-based highlighter (highlight command) |
| `about-formatting.sh` | ABOUT | Renders markdown via `markdown` or `rst2html` |
| `html-converters/md2html` | ABOUT | Standalone markdown-to-HTML converter |
| `html-converters/rst2html` | ABOUT | reStructuredText-to-HTML converter |
| `html-converters/txt2html` | ABOUT | Plain text to HTML converter |
| `email-gravatar.py` | EMAIL | Adds gravatar avatars |
| `email-libravatar.lua` | EMAIL | Lua-based libravatar integration |
| `simple-hierarchical-auth.lua` | AUTH | Lua path-based authentication |

## Error Handling

If an exec filter's child process exits with a non-zero status, `close()`
returns that status code.  The calling code can check this to fall back to
unfiltered output.

If a Lua filter throws an error, the error message is logged via
`die("lua error")` and the filter is aborted.

## Performance Considerations

- **Exec filters** have per-invocation fork/exec overhead.  For high-traffic
  sites, consider Lua filters or enabling the response cache.
- **Lua filters** run in-process with no fork overhead but require Lua support
  to be compiled in.
- Filters are not called when serving cached responses — the cached output
  already includes the filtered content.
