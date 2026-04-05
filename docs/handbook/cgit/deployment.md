# cgit — Deployment Guide

## Overview

cgit runs as a CGI application under a web server.  This guide covers
compilation, installation, web server configuration, and production tuning.

## Prerequisites

Build dependencies:
- GCC or Clang (C99 compiler)
- GNU Make
- OpenSSL or compatible TLS library (for libgit HTTPS)
- zlib (for git object decompression)
- Optional: Lua or LuaJIT (for Lua filters)
- Optional: pkg-config (for Lua detection)

Runtime dependencies:
- A CGI-capable web server (Apache, Nginx+fcgiwrap, lighttpd)
- Git repositories on the filesystem

## Building

```bash
# Clone/download the source
cd cgit/

# Build with defaults
make

# Or with custom settings
make prefix=/usr CGIT_SCRIPT_PATH=/var/www/cgi-bin \
     CGIT_CONFIG=/etc/cgitrc CACHE_ROOT=/var/cache/cgit

# Install
make install
```

### Build Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `prefix` | `/usr/local` | Installation prefix |
| `CGIT_SCRIPT_PATH` | `$(prefix)/lib/cgit` | CGI binary directory |
| `CGIT_DATA_PATH` | `$(prefix)/share/cgit` | Static files (CSS, images) |
| `CGIT_CONFIG` | `/etc/cgitrc` | Default config file path |
| `CACHE_ROOT` | `/var/cache/cgit` | Default cache directory |
| `CGIT_SCRIPT_NAME` | `"/"` | Default CGI script name |
| `NO_LUA` | (unset) | Set to 1 to disable Lua |

### Installed Files

```
$(CGIT_SCRIPT_PATH)/cgit.cgi    # CGI binary
$(CGIT_DATA_PATH)/cgit.css      # Stylesheet
$(CGIT_DATA_PATH)/cgit.js       # JavaScript
$(CGIT_DATA_PATH)/cgit.png      # Logo image
$(CGIT_DATA_PATH)/robots.txt    # Robots exclusion file
```

## Apache Configuration

### CGI Module

```apache
# Enable CGI
LoadModule cgi_module modules/mod_cgi.so

# Basic CGI setup
ScriptAlias /cgit/ /usr/lib/cgit/cgit.cgi/
Alias /cgit-data/ /usr/share/cgit/

<Directory "/usr/lib/cgit/">
    AllowOverride None
    Options +ExecCGI
    Require all granted
</Directory>

<Directory "/usr/share/cgit/">
    AllowOverride None
    Require all granted
</Directory>
```

### URL Rewriting (Clean URLs)

```apache
# Enable clean URLs via mod_rewrite
RewriteEngine On
RewriteRule ^/cgit/(.*)$ /usr/lib/cgit/cgit.cgi/$1 [PT]
```

With corresponding cgitrc:

```ini
virtual-root=/cgit/
css=/cgit-data/cgit.css
logo=/cgit-data/cgit.png
```

## Nginx Configuration

Nginx does not support CGI natively.  Use `fcgiwrap` or `spawn-fcgi`:

### With fcgiwrap

```bash
# Install fcgiwrap
# Start it (systemd, OpenRC, or manual)
fcgiwrap -s unix:/run/fcgiwrap.sock &
```

```nginx
server {
    listen 80;
    server_name git.example.com;

    root /usr/share/cgit;

    # Serve static files directly
    location /cgit-data/ {
        alias /usr/share/cgit/;
    }

    # Pass CGI requests to fcgiwrap
    location /cgit {
        include fastcgi_params;
        fastcgi_param SCRIPT_FILENAME /usr/lib/cgit/cgit.cgi;
        fastcgi_param PATH_INFO $uri;
        fastcgi_param QUERY_STRING $args;
        fastcgi_param HTTP_HOST $server_name;
        fastcgi_pass unix:/run/fcgiwrap.sock;
    }
}
```

### With spawn-fcgi

```bash
spawn-fcgi -s /run/cgit.sock -n -- /usr/bin/fcgiwrap
```

## lighttpd Configuration

```lighttpd
server.modules += ("mod_cgi", "mod_alias", "mod_rewrite")

alias.url = (
    "/cgit-data/" => "/usr/share/cgit/",
    "/cgit/"      => "/usr/lib/cgit/cgit.cgi"
)

cgi.assign = (
    "cgit.cgi" => ""
)

url.rewrite-once = (
    "^/cgit/(.*)$" => "/cgit/cgit.cgi/$1"
)
```

## Configuration File

Create `/etc/cgitrc`:

```ini
# Site identity
root-title=My Git Server
root-desc=Git repository browser
css=/cgit-data/cgit.css
logo=/cgit-data/cgit.png
favicon=/cgit-data/favicon.ico

# URL routing
virtual-root=/cgit/

# Features
enable-commit-graph=1
enable-blame=1
enable-http-clone=1
enable-index-links=1
snapshots=tar.gz tar.xz zip
max-stats=quarter

# Caching (recommended for production)
cache-size=1000
cache-root=/var/cache/cgit
cache-root-ttl=5
cache-repo-ttl=5
cache-static-ttl=-1

# Repository discovery
scan-path=/srv/git/
section-from-path=1
enable-git-config=1

# Filters
source-filter=exec:/usr/lib/cgit/filters/syntax-highlighting.py
about-filter=exec:/usr/lib/cgit/filters/about-formatting.sh
```

## Cache Directory Setup

```bash
# Create cache directory
mkdir -p /var/cache/cgit

# Set ownership to web server user
chown www-data:www-data /var/cache/cgit
chmod 700 /var/cache/cgit

# Optional: periodic cleanup cron job
echo "*/30 * * * * find /var/cache/cgit -type f -mmin +60 -delete" | \
    crontab -u www-data -
```

## Repository Permissions

The web server user needs read access to all git repositories:

```bash
# Option 1: Add web server user to git group
usermod -aG git www-data

# Option 2: Set directory permissions
chmod -R g+rX /srv/git/

# Option 3: Use ACLs
setfacl -R -m u:www-data:rX /srv/git/
setfacl -R -d -m u:www-data:rX /srv/git/
```

## HTTPS Setup

For production, serve cgit over HTTPS:

```nginx
server {
    listen 443 ssl;
    server_name git.example.com;

    ssl_certificate /etc/ssl/certs/git.example.com.pem;
    ssl_certificate_key /etc/ssl/private/git.example.com.key;

    # ... cgit configuration ...
}

server {
    listen 80;
    server_name git.example.com;
    return 301 https://$server_name$request_uri;
}
```

## Performance Tuning

### Enable Caching

The response cache is essential for performance:

```ini
cache-size=1000          # number of cache entries
cache-root-ttl=5         # repo list: 5 minutes
cache-repo-ttl=5         # repo pages: 5 minutes
cache-static-ttl=-1      # static content: forever
cache-about-ttl=15       # about pages: 15 minutes
```

### Limit Resource Usage

```ini
max-repo-count=100       # repos per page
max-commit-count=50      # commits per page
max-blob-size=512        # max blob display (KB)
max-message-length=120   # truncate long subjects
max-repodesc-length=80   # truncate descriptions
```

### Use Lua Filters

Lua filters avoid fork/exec overhead:

```ini
source-filter=lua:/usr/share/cgit/filters/syntax-highlight.lua
email-filter=lua:/usr/share/cgit/filters/email-libravatar.lua
```

### Optimize Git Access

```bash
# Run periodic git gc on repositories
for repo in /srv/git/*.git; do
    git -C "$repo" gc --auto
done

# Ensure pack files are optimized
for repo in /srv/git/*.git; do
    git -C "$repo" repack -a -d
done
```

## Monitoring

### Check Cache Status

```bash
# Count cache entries
ls /var/cache/cgit/ | wc -l

# Check cache hit rate (if access logs are enabled)
grep "cgit.cgi" /var/log/nginx/access.log | tail -100
```

### Health Check

```bash
# Verify cgit is responding
curl -s -o /dev/null -w "%{http_code}" http://localhost/cgit/
```

## Docker Deployment

```dockerfile
FROM alpine:latest

RUN apk add --no-cache \
    git make gcc musl-dev openssl-dev zlib-dev lua5.3-dev \
    fcgiwrap nginx

COPY cgit/ /build/cgit/
WORKDIR /build/cgit
RUN make && make install

COPY cgitrc /etc/cgitrc
COPY nginx.conf /etc/nginx/conf.d/cgit.conf

EXPOSE 80
CMD ["sh", "-c", "fcgiwrap -s unix:/run/fcgiwrap.sock & nginx -g 'daemon off;'"]
```

## systemd Service

```ini
# /etc/systemd/system/fcgiwrap-cgit.service
[Unit]
Description=fcgiwrap for cgit
After=network.target

[Service]
ExecStart=/usr/bin/fcgiwrap -s unix:/run/fcgiwrap.sock
User=www-data
Group=www-data

[Install]
WantedBy=multi-user.target
```

## Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|----------|
| 500 Internal Server Error | CGI binary not executable | `chmod +x cgit.cgi` |
| Blank page | Missing CSS path | Check `css=` directive |
| No repositories shown | Wrong `scan-path` | Verify path and permissions |
| Cache errors | Permission denied | Fix cache dir ownership |
| Lua filter fails | Lua not compiled in | Rebuild without `NO_LUA` |
| Clone fails | `enable-http-clone=0` | Set to `1` |
| Missing styles | Static file alias wrong | Check web server alias config |
| Timeout on large repos | No caching | Enable `cache-size` |
