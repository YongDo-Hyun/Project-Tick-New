# cgit — Authentication

## Overview

cgit supports cookie-based authentication through the `auth-filter`
mechanism.  The authentication system intercepts requests before page
rendering and delegates all credential validation to an external filter
(exec or Lua script).

Source file: `cgit.c` (authentication hooks), `filter.c` (filter execution).

## Architecture

Authentication is entirely filter-driven.  cgit itself stores no credentials,
sessions, or user databases.  The auth filter is responsible for:

1. Rendering login forms
2. Validating credentials
3. Setting/reading session cookies
4. Determining authorization per-repository

## Configuration

```ini
auth-filter=lua:/path/to/auth.lua
# or
auth-filter=exec:/path/to/auth.sh
```

The auth filter type is `AUTH_FILTER` (constant `4`) and receives 12
arguments.

## Authentication Flow

### Request Processing in `cgit.c`

Authentication is checked in `process_request()` after URL parsing and
command dispatch:

```c
/* In process_request() */
if (ctx.cfg.auth_filter) {
    /* Step 1: Check current authentication state */
    authenticate_cookie();

    /* Step 2: Handle POST login attempts */
    if (ctx.env.request_method &&
        !strcmp(ctx.env.request_method, "POST"))
        authenticate_post();

    /* Step 3: Run the auth filter to decide access */
    cmd->fn(&ctx);
}
```

### `authenticate_cookie()`

Opens the auth filter to check the current session cookie:

```c
static void authenticate_cookie(void)
{
    /* Open auth filter with current request context */
    cgit_open_filter(ctx.cfg.auth_filter,
        ctx.env.http_cookie,      /* current cookies */
        ctx.env.request_method,   /* GET/POST */
        ctx.env.query_string,     /* full query */
        ctx.env.http_referer,     /* referer header */
        ctx.env.path_info,        /* request path */
        ctx.env.http_host,        /* hostname */
        ctx.env.https ? "1" : "0", /* HTTPS flag */
        ctx.qry.repo,             /* repository name */
        ctx.qry.page,             /* page/command */
        ctx.env.http_accept,      /* accept header */
        "cookie"                  /* authentication phase */
    );
    /* Read filter's response to determine auth state */
    ctx.env.authenticated = /* filter exit code */;
    cgit_close_filter(ctx.cfg.auth_filter);
}
```

### `authenticate_post()`

Handles login form submissions:

```c
static void authenticate_post(void)
{
    /* Read POST body for credentials */
    /* Open auth filter with phase="post" */
    cgit_open_filter(ctx.cfg.auth_filter,
        /* ... same 11 args ... */
        "post"                    /* authentication phase */
    );
    /* Filter processes credentials, may set cookies */
    cgit_close_filter(ctx.cfg.auth_filter);
}
```

### Authorization Check

After authentication, the auth filter is called again before rendering each
page to determine if the authenticated user has access to the requested
repository and page:

```c
static int open_auth_filter(const char *repo, const char *page)
{
    cgit_open_filter(ctx.cfg.auth_filter,
        /* ... request context ... */
        "authorize"               /* authorization phase */
    );
    int authorized = cgit_close_filter(ctx.cfg.auth_filter);
    return authorized == 0;  /* 0 = authorized */
}
```

## Auth Filter Arguments

The auth filter receives 12 arguments in total:

| # | Argument | Description |
|---|----------|-------------|
| 1 | `filter_cmd` | The filter command itself |
| 2 | `http_cookie` | Raw `HTTP_COOKIE` header value |
| 3 | `request_method` | HTTP method (`GET`, `POST`) |
| 4 | `query_string` | Raw query string |
| 5 | `http_referer` | HTTP Referer header |
| 6 | `path_info` | PATH_INFO from CGI |
| 7 | `http_host` | Hostname |
| 8 | `https` | `"1"` if HTTPS, `"0"` if HTTP |
| 9 | `repo` | Repository URL |
| 10 | `page` | Page/command name |
| 11 | `http_accept` | HTTP Accept header |
| 12 | `phase` | `"cookie"`, `"post"`, or `"authorize"` |

## Filter Phases

### `cookie` Phase

Called on every request.  The filter should:
1. Read the session cookie from argument 2
2. Validate the session
3. Return exit code 0 if authenticated, non-zero otherwise

### `post` Phase

Called when the request method is POST.  The filter should:
1. Read POST body from stdin
2. Validate credentials
3. If valid, output a `Set-Cookie` header
4. Output a redirect response (302)

### `authorize` Phase

Called after authentication to check per-repository access.  The filter
should:
1. Check if the authenticated user can access the requested repo/page
2. Return exit code 0 if authorized
3. Return non-zero to deny access (cgit will show an error page)

## Filter Return Codes

| Exit Code | Meaning |
|-----------|---------|
| 0 | Success (authenticated/authorized) |
| Non-zero | Failure (unauthenticated/unauthorized) |

## Environment Variables

The auth filter also has access to standard CGI environment variables:

```c
struct cgit_environment {
    const char *cgit_config;       /* $CGIT_CONFIG */
    const char *http_host;         /* $HTTP_HOST */
    const char *https;             /* $HTTPS */
    const char *no_http;           /* $NO_HTTP */
    const char *http_cookie;       /* $HTTP_COOKIE */
    const char *request_method;    /* $REQUEST_METHOD */
    const char *query_string;      /* $QUERY_STRING */
    const char *http_referer;      /* $HTTP_REFERER */
    const char *path_info;         /* $PATH_INFO */
    const char *script_name;       /* $SCRIPT_NAME */
    const char *server_name;       /* $SERVER_NAME */
    const char *server_port;       /* $SERVER_PORT */
    const char *http_accept;       /* $HTTP_ACCEPT */
    int authenticated;             /* set by auth filter */
};
```

## Shipped Auth Filter

cgit ships a Lua-based hierarchical authentication filter:

### `filters/simple-hierarchical-auth.lua`

This filter implements path-based access control using a simple user
database and repository permission map.

Features:
- Cookie-based session management
- Per-repository access control
- Hierarchical path matching
- Password hashing

Usage:

```ini
auth-filter=lua:/usr/lib/cgit/filters/simple-hierarchical-auth.lua
```

## Cache Interaction

Authentication affects cache keys.  The cache key includes the
authentication state and cookie:

```c
static const char *cache_key(void)
{
    return fmt("%s?%s?%s?%s?%s",
        ctx.qry.raw,
        ctx.env.http_host,
        ctx.env.https ? "1" : "0",
        ctx.env.authenticated ? "1" : "0",
        ctx.env.http_cookie ? ctx.env.http_cookie : "");
}
```

This ensures that:
- Authenticated and unauthenticated users get separate cache entries
- Different authenticated users (different cookies) get separate entries
- The cache never leaks restricted content to unauthorized users

## Security Considerations

1. **HTTPS**: Always use HTTPS when authentication is enabled to protect
   cookies and credentials in transit
2. **Cookie flags**: Auth filter scripts should set `Secure`, `HttpOnly`,
   and `SameSite` cookie flags
3. **Session expiry**: Implement session timeouts in the auth filter
4. **Password storage**: Never store passwords in plain text; use bcrypt or
   similar hashing
5. **CSRF protection**: The auth filter should implement CSRF tokens for
   POST login forms
6. **Cache poisoning**: The cache key includes auth state, but ensure the
   auth filter is deterministic for the same cookie

## Disabling Authentication

By default, no auth filter is configured and all repositories are publicly
accessible.  To restrict access, set up the auth filter and optionally
combine with `strict-export` for file-based visibility control.

## Example: Custom Auth Filter (Shell)

```bash
#!/bin/bash
# Simple auth filter skeleton
PHASE="${12}"

case "$PHASE" in
    cookie)
        COOKIE="$2"
        if validate_session "$COOKIE"; then
            exit 0   # authenticated
        fi
        exit 1       # not authenticated
        ;;
    post)
        read -r POST_BODY
        # Parse username/password from POST_BODY
        # Validate credentials
        # Set cookie header
        echo "Status: 302 Found"
        echo "Set-Cookie: session=TOKEN; HttpOnly; Secure"
        echo "Location: $6"
        echo
        exit 0
        ;;
    authorize)
        REPO="$9"
        # Check if current user can access $REPO
        exit 0  # authorized
        ;;
esac
```
