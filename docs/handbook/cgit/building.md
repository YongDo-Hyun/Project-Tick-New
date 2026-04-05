# cgit — Building

## Prerequisites

| Dependency | Required | Purpose |
|-----------|----------|---------|
| GCC or Clang | Yes | C compiler (C99) |
| GNU Make | Yes | Build system |
| OpenSSL (libcrypto) | Yes | SHA-1 hash implementation (`SHA1_HEADER = <openssl/sha.h>`) |
| zlib | Yes | Git object compression |
| libcurl | No | Not used — `NO_CURL=1` is passed by cgit.mk |
| Lua or LuaJIT | No | Lua filter support; auto-detected via pkg-config |
| asciidoc / a2x | No | Man page / HTML / PDF documentation generation |
| Python | No | Git's test harness (for `make test`) |

## Build System Overview

cgit uses a two-stage build that embeds itself within Git's build infrastructure:

```
cgit/Makefile
  └── make -C git -f ../cgit.mk ../cgit
        └── git/Makefile (included by cgit.mk)
              └── Compile cgit objects + link against libgit.a
```

### Stage 1: Top-Level Makefile

The top-level `Makefile` lives in `cgit/` and defines all user-configurable
variables:

```makefile
CGIT_VERSION = 0.0.5-1-Project-Tick
CGIT_SCRIPT_NAME = cgit.cgi
CGIT_SCRIPT_PATH = /var/www/htdocs/cgit
CGIT_DATA_PATH = $(CGIT_SCRIPT_PATH)
CGIT_CONFIG = /etc/cgitrc
CACHE_ROOT = /var/cache/cgit
prefix = /usr/local
libdir = $(prefix)/lib
filterdir = $(libdir)/cgit/filters
docdir = $(prefix)/share/doc/cgit
mandir = $(prefix)/share/man
SHA1_HEADER = <openssl/sha.h>
GIT_VER = 2.46.0
GIT_URL = https://www.kernel.org/pub/software/scm/git/git-$(GIT_VER).tar.xz
```

The main `cgit` target delegates to:

```makefile
cgit:
	$(QUIET_SUBDIR0)git $(QUIET_SUBDIR1) -f ../cgit.mk ../cgit NO_CURL=1
```

This enters the `git/` subdirectory and runs `cgit.mk` from there, prefixing
all cgit source paths with `../`.

### Stage 2: cgit.mk

`cgit.mk` is run inside the `git/` directory so it can `include Makefile` to
inherit Git's build variables (`CC`, `CFLAGS`, linker flags, OS detection via
`config.mak.uname`, etc.).

Key sections:

#### Version tracking

```makefile
$(CGIT_PREFIX)VERSION: force-version
	@cd $(CGIT_PREFIX) && '$(SHELL_PATH_SQ)' ./gen-version.sh "$(CGIT_VERSION)"
```

The `gen-version.sh` script writes a `VERSION` file that is included by the
build.  Only `cgit.o` references `CGIT_VERSION`, so only that object is rebuilt
when the version changes.

#### CGIT_CFLAGS

```makefile
CGIT_CFLAGS += -DCGIT_CONFIG='"$(CGIT_CONFIG)"'
CGIT_CFLAGS += -DCGIT_SCRIPT_NAME='"$(CGIT_SCRIPT_NAME)"'
CGIT_CFLAGS += -DCGIT_CACHE_ROOT='"$(CACHE_ROOT)"'
```

These compile-time constants are used in `cgit.c` as default values in
`prepare_context()`.

#### Lua detection

```makefile
LUA_PKGCONFIG := $(shell for pc in luajit lua lua5.2 lua5.1; do \
    $(PKG_CONFIG) --exists $$pc 2>/dev/null && echo $$pc && break; \
done)
```

If Lua is found, its `--cflags` and `--libs` are appended to `CGIT_CFLAGS` and
`CGIT_LIBS`.  If not found, `NO_LUA=YesPlease` is set and `-DNO_LUA` is added.

#### Linux sendfile

```makefile
ifeq ($(uname_S),Linux)
    HAVE_LINUX_SENDFILE = YesPlease
endif

ifdef HAVE_LINUX_SENDFILE
    CGIT_CFLAGS += -DHAVE_LINUX_SENDFILE
endif
```

This enables the `sendfile()` syscall in `cache.c` for zero-copy writes from
cache files to stdout.

#### Object files

All cgit source files are listed explicitly:

```makefile
CGIT_OBJ_NAMES += cgit.o cache.o cmd.o configfile.o filter.o html.o
CGIT_OBJ_NAMES += parsing.o scan-tree.o shared.o
CGIT_OBJ_NAMES += ui-atom.o ui-blame.o ui-blob.o ui-clone.o ui-commit.o
CGIT_OBJ_NAMES += ui-diff.o ui-log.o ui-patch.o ui-plain.o ui-refs.o
CGIT_OBJ_NAMES += ui-repolist.o ui-shared.o ui-snapshot.o ui-ssdiff.o
CGIT_OBJ_NAMES += ui-stats.o ui-summary.o ui-tag.o ui-tree.o
```

The prefixed paths (`CGIT_OBJS := $(addprefix $(CGIT_PREFIX),$(CGIT_OBJ_NAMES))`)
point back to the `cgit/` directory from inside `git/`.

## Quick Build

```bash
cd cgit

# Download the vendored Git source (required on first build)
make get-git

# Build cgit binary
make -j$(nproc)
```

The output is a single binary named `cgit` in the `cgit/` directory.

## Build Variables Reference

| Variable | Default | Description |
|----------|---------|-------------|
| `CGIT_VERSION` | `0.0.5-1-Project-Tick` | Compiled-in version string |
| `CGIT_SCRIPT_NAME` | `cgit.cgi` | Name of the installed CGI binary |
| `CGIT_SCRIPT_PATH` | `/var/www/htdocs/cgit` | CGI binary install directory |
| `CGIT_DATA_PATH` | `$(CGIT_SCRIPT_PATH)` | Static assets (CSS, JS, images) directory |
| `CGIT_CONFIG` | `/etc/cgitrc` | Default config file path (compiled in) |
| `CACHE_ROOT` | `/var/cache/cgit` | Default cache directory (compiled in) |
| `prefix` | `/usr/local` | Install prefix |
| `libdir` | `$(prefix)/lib` | Library directory |
| `filterdir` | `$(libdir)/cgit/filters` | Filter scripts install directory |
| `docdir` | `$(prefix)/share/doc/cgit` | Documentation directory |
| `mandir` | `$(prefix)/share/man` | Man page directory |
| `SHA1_HEADER` | `<openssl/sha.h>` | SHA-1 implementation header |
| `GIT_VER` | `2.46.0` | Git version to download and vendor |
| `GIT_URL` | `https://...git-$(GIT_VER).tar.xz` | Git source download URL |
| `NO_LUA` | (unset) | Set to any value to disable Lua support |
| `LUA_PKGCONFIG` | (auto-detected) | Explicit pkg-config name for Lua |
| `NO_C99_FORMAT` | (unset) | Define if your printf lacks `%zu`, `%lld` etc. |
| `HAVE_LINUX_SENDFILE` | (auto on Linux) | Enable `sendfile()` in cache |
| `V` | (unset) | Set to `1` for verbose build output |

Overrides can be placed in a `cgit.conf` file (included by both `Makefile` and
`cgit.mk` via `-include cgit.conf`).

## Installation

```bash
make install              # Install binary and static assets
make install-doc          # Install man pages, HTML docs, PDF docs
make install-man          # Man pages only
make install-html         # HTML docs only
make install-pdf          # PDF docs only
```

### Installed files

| Path | Mode | Source |
|------|------|--------|
| `$(CGIT_SCRIPT_PATH)/$(CGIT_SCRIPT_NAME)` | 0755 | `cgit` binary |
| `$(CGIT_DATA_PATH)/cgit.css` | 0644 | Default stylesheet |
| `$(CGIT_DATA_PATH)/cgit.js` | 0644 | Client-side JavaScript |
| `$(CGIT_DATA_PATH)/cgit.png` | 0644 | Default logo |
| `$(CGIT_DATA_PATH)/favicon.ico` | 0644 | Default favicon |
| `$(CGIT_DATA_PATH)/robots.txt` | 0644 | Robots exclusion file |
| `$(filterdir)/*` | (varies) | Filter scripts from `filters/` |
| `$(mandir)/man5/cgitrc.5` | 0644 | Man page (if `install-man`) |

## Make Targets

| Target | Description |
|--------|-------------|
| `all` | Build the cgit binary (default) |
| `cgit` | Explicit build target |
| `test` | Build everything (`all` target on git) then run `tests/` |
| `install` | Install binary, CSS, JS, images, filters |
| `install-doc` | Install man pages + HTML + PDF |
| `install-man` | Man pages only |
| `install-html` | HTML docs only |
| `install-pdf` | PDF docs only |
| `clean` | Remove cgit objects, VERSION, CGIT-CFLAGS, tags |
| `cleanall` | `clean` + `make -C git clean` |
| `clean-doc` | Remove generated doc files |
| `get-git` | Download and extract Git source into `git/` |
| `tags` | Generate ctags for all `*.[ch]` files |
| `sparse` | Run `sparse` static analysis via cgit.mk |
| `uninstall` | Remove installed binary and assets |
| `uninstall-doc` | Remove installed documentation |

## Documentation Generation

Man pages are generated from `cgitrc.5.txt` using `asciidoc`/`a2x`:

```makefile
MAN5_TXT = $(wildcard *.5.txt)
DOC_MAN5 = $(patsubst %.txt,%,$(MAN5_TXT))
DOC_HTML = $(patsubst %.txt,%.html,$(MAN_TXT))
DOC_PDF  = $(patsubst %.txt,%.pdf,$(MAN_TXT))

%.5 : %.5.txt
	a2x -f manpage $<

$(DOC_HTML): %.html : %.txt
	$(TXT_TO_HTML) -o $@+ $< && mv $@+ $@

$(DOC_PDF): %.pdf : %.txt
	a2x -f pdf cgitrc.5.txt
```

## Cross-Compilation

For cross-compiling (e.g. targeting MinGW on Linux):

```bash
make CC=x86_64-w64-mingw32-gcc
```

The `toolchain-mingw32.cmake` file in the repository is for CMake-based
projects; cgit itself uses Make exclusively.

## Customizing the Build

Create a `cgit.conf` file alongside the Makefile:

```makefile
# cgit.conf — local build overrides
CGIT_VERSION = 1.0.0-custom
CGIT_CONFIG = /usr/local/etc/cgitrc
CACHE_ROOT = /tmp/cgit-cache
NO_LUA = 1
```

This file is `-include`d by both `Makefile` and `cgit.mk`, so it applies to
all build stages.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `make: *** No rule to make target 'git/Makefile'` | Run `make get-git` first |
| `lua.h: No such file or directory` | Install Lua dev package or set `NO_LUA=1` |
| `openssl/sha.h: No such file or directory` | Install `libssl-dev` / `openssl-devel` |
| `sendfile: undefined reference` | Set `HAVE_LINUX_SENDFILE=` (empty) on non-Linux |
| Build fails with `redefinition of 'struct cache_slot'` | Git's `cache.h` conflict — cgit uses `CGIT_CACHE_H` guard |
| `dlsym: symbol not found: write` | Lua filter's `write()` interposition requires `-ldl` (auto on Linux) |
| Version shows as `unknown` | Run `./gen-version.sh "$(CGIT_VERSION)"` or check `VERSION` file |
