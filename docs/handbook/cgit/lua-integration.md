# cgit — Lua Integration

## Overview

cgit supports Lua as an in-process scripting language for content filters.
Lua filters avoid the fork/exec overhead of shell-based filters and have
direct access to cgit's HTML output functions.  Lua support is optional and
auto-detected at compile time.

Source files: `filter.c` (Lua filter implementation), `cgit.mk` (Lua detection).

## Compile-Time Detection

Lua support is detected by `cgit.mk` using `pkg-config`:

```makefile
ifndef NO_LUA
LUAPKGS := luajit lua lua5.2 lua5.1
LUAPKG := $(shell for p in $(LUAPKGS); do \
    $(PKG_CONFIG) --exists $$p 2>/dev/null && echo $$p && break; done)
ifneq ($(LUAPKG),)
    CGIT_CFLAGS += -DHAVE_LUA $(shell $(PKG_CONFIG) --cflags $(LUAPKG))
    CGIT_LIBS += $(shell $(PKG_CONFIG) --libs $(LUAPKG))
endif
endif
```

Detection order: `luajit` → `lua` → `lua5.2` → `lua5.1`.

To disable Lua even when available:

```bash
make NO_LUA=1
```

The `HAVE_LUA` preprocessor define gates all Lua-related code:

```c
#ifdef HAVE_LUA
/* Lua filter implementation */
#else
/* stub: cgit_new_filter() returns NULL for lua: prefix */
#endif
```

## Lua Filter Structure

```c
struct cgit_lua_filter {
    struct cgit_filter base;   /* common filter fields */
    char *script_file;         /* path to Lua script */
    lua_State *lua_state;      /* Lua interpreter state */
};
```

The `lua_State` is lazily initialized on first use and reused for subsequent
invocations of the same filter.

## C API Exposed to Lua

cgit registers these C functions in the Lua environment:

### `html(str)`

Writes raw HTML to stdout (no escaping):

```c
static int lua_html(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    html(str);
    return 0;
}
```

### `html_txt(str)`

Writes HTML-escaped text:

```c
static int lua_html_txt(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    html_txt(str);
    return 0;
}
```

### `html_attr(str)`

Writes attribute-escaped text:

```c
static int lua_html_attr(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    html_attr(str);
    return 0;
}
```

### `html_url_path(str)`

Writes a URL-encoded path:

```c
static int lua_html_url_path(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    html_url_path(str);
    return 0;
}
```

### `html_url_arg(str)`

Writes a URL-encoded query argument:

```c
static int lua_html_url_arg(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    html_url_arg(str);
    return 0;
}
```

### `html_include(filename)`

Includes a file's contents in the output:

```c
static int lua_html_include(lua_State *L)
{
    const char *filename = luaL_checkstring(L, 1);
    html_include(filename);
    return 0;
}
```

## Lua Filter Lifecycle

### Initialization

On first `open()`, the Lua state is created and the script is loaded:

```c
static int open_lua_filter(struct cgit_filter *base, ...)
{
    struct cgit_lua_filter *f = (struct cgit_lua_filter *)base;

    if (!f->lua_state) {
        /* Create new Lua state */
        f->lua_state = luaL_newstate();
        luaL_openlibs(f->lua_state);

        /* Register C functions */
        lua_pushcfunction(f->lua_state, lua_html);
        lua_setglobal(f->lua_state, "html");
        lua_pushcfunction(f->lua_state, lua_html_txt);
        lua_setglobal(f->lua_state, "html_txt");
        lua_pushcfunction(f->lua_state, lua_html_attr);
        lua_setglobal(f->lua_state, "html_attr");
        lua_pushcfunction(f->lua_state, lua_html_url_path);
        lua_setglobal(f->lua_state, "html_url_path");
        lua_pushcfunction(f->lua_state, lua_html_url_arg);
        lua_setglobal(f->lua_state, "html_url_arg");
        lua_pushcfunction(f->lua_state, lua_html_include);
        lua_setglobal(f->lua_state, "include");

        /* Load and execute the script file */
        if (luaL_dofile(f->lua_state, f->script_file))
            die("lua error: %s",
                lua_tostring(f->lua_state, -1));
    }

    /* Redirect stdout writes to lua write() function */

    /* Call filter_open() with filter-specific arguments */
    lua_getglobal(f->lua_state, "filter_open");
    /* push arguments from va_list */
    lua_call(f->lua_state, nargs, 0);

    return 0;
}
```

### Data Flow

While the filter is open, data written to stdout is intercepted via a custom
`write()` function:

```c
/* The fprintf callback for Lua filters */
static void lua_fprintf(struct cgit_filter *base, FILE *f,
                        const char *fmt, ...)
{
    struct cgit_lua_filter *lf = (struct cgit_lua_filter *)base;
    /* format the string */
    /* call the Lua write() function with the formatted text */
    lua_getglobal(lf->lua_state, "write");
    lua_pushstring(lf->lua_state, buf);
    lua_call(lf->lua_state, 1, 0);
}
```

### Close

```c
static int close_lua_filter(struct cgit_filter *base)
{
    struct cgit_lua_filter *f = (struct cgit_lua_filter *)base;

    /* Call filter_close() */
    lua_getglobal(f->lua_state, "filter_close");
    lua_call(f->lua_state, 0, 1);

    /* Get return code */
    int rc = lua_tointeger(f->lua_state, -1);
    lua_pop(f->lua_state, 1);

    return rc;
}
```

### Cleanup

```c
static void cleanup_lua_filter(struct cgit_filter *base)
{
    struct cgit_lua_filter *f = (struct cgit_lua_filter *)base;
    if (f->lua_state)
        lua_close(f->lua_state);
}
```

## Lua Script Interface

### Required Functions

A Lua filter script must define these functions:

```lua
function filter_open(...)
    -- Called when the filter opens
    -- Arguments are filter-type specific
end

function write(str)
    -- Called with content chunks to process
    -- Transform and output using html() functions
end

function filter_close()
    -- Called when filtering is complete
    return 0  -- return exit code
end
```

### Available Global Functions

| Function | Description |
|----------|-------------|
| `html(str)` | Output raw HTML |
| `html_txt(str)` | Output HTML-escaped text |
| `html_attr(str)` | Output attribute-escaped text |
| `html_url_path(str)` | Output URL-path-encoded text |
| `html_url_arg(str)` | Output URL-argument-encoded text |
| `include(filename)` | Include file contents in output |

All standard Lua libraries are available (`string`, `table`, `math`, `io`,
`os`, etc.).

## Example Filters

### Source Highlighting Filter

```lua
-- syntax-highlighting.lua
local filename = ""
local buffer = {}

function filter_open(fn)
    filename = fn
    buffer = {}
end

function write(str)
    table.insert(buffer, str)
end

function filter_close()
    local content = table.concat(buffer)
    local ext = filename:match("%.(%w+)$") or ""

    -- Simple keyword highlighting
    local keywords = {
        ["function"] = true, ["local"] = true,
        ["if"] = true, ["then"] = true,
        ["end"] = true, ["return"] = true,
        ["for"] = true, ["while"] = true,
        ["do"] = true, ["else"] = true,
    }

    html("<pre><code>")
    for line in content:gmatch("([^\n]*)\n?") do
        html_txt(line)
        html("\n")
    end
    html("</code></pre>")

    return 0
end
```

### Email Obfuscation Filter

```lua
-- email-obfuscate.lua
function filter_open(email, page)
    -- email = the email address
    -- page = current page name
end

function write(str)
    -- Replace @ with [at] for display
    local obfuscated = str:gsub("@", " [at] ")
    html_txt(obfuscated)
end

function filter_close()
    return 0
end
```

### About/README Filter

```lua
-- about-markdown.lua
local buffer = {}

function filter_open(filename)
    buffer = {}
end

function write(str)
    table.insert(buffer, str)
end

function filter_close()
    local content = table.concat(buffer)
    -- Process markdown (using a Lua markdown library)
    -- or shell out to a converter
    local handle = io.popen("cmark", "w")
    handle:write(content)
    local result = handle:read("*a")
    handle:close()
    html(result)
    return 0
end
```

### Auth Filter (Lua)

```lua
-- auth.lua
-- The auth filter receives 12 arguments
function filter_open(cookie, method, query, referer, path,
                     host, https, repo, page, accept, phase)
    if phase == "cookie" then
        -- Validate session cookie
        if valid_session(cookie) then
            return 0  -- authenticated
        end
        return 1  -- not authenticated
    elseif phase == "post" then
        -- Handle login form submission
    elseif phase == "authorize" then
        -- Check repository access
    end
end

function write(str)
    html(str)
end

function filter_close()
    return 0
end
```

## Performance

Lua filters offer significant performance advantages over exec filters:

| Aspect | Exec Filter | Lua Filter |
|--------|-------------|------------|
| Startup | fork() + exec() per request | One-time Lua state creation |
| Process | New process per invocation | In-process |
| Memory | Separate address space | Shared memory |
| Latency | ~1-5ms fork overhead | ~0.01ms function call |
| Libraries | Any language | Lua libraries only |

## Limitations

- Lua scripts run in the same process as cgit — a crash in the script
  crashes cgit
- Standard Lua I/O functions (`print`, `io.write`) bypass cgit's output
  pipeline — use `html()` and friends instead
- The Lua state persists between invocations within the same CGI process,
  but CGI processes are typically short-lived
- Error handling is via `die()` — a Lua error terminates the CGI process

## Configuration

```ini
# Use Lua filter for source highlighting
source-filter=lua:/usr/share/cgit/filters/syntax-highlight.lua

# Use Lua filter for about pages
about-filter=lua:/usr/share/cgit/filters/about-markdown.lua

# Use Lua filter for authentication
auth-filter=lua:/usr/share/cgit/filters/simple-hierarchical-auth.lua

# Use Lua filter for email display
email-filter=lua:/usr/share/cgit/filters/email-libravatar.lua
```
