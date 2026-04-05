# cgit — Testing

## Overview

cgit has a shell-based test suite in the `tests/` directory.  Tests use
Git's own test framework (`test-lib.sh`) and exercise cgit by invoking the
CGI binary with simulated HTTP requests.

Source location: `cgit/tests/`.

## Test Framework

The test harness is built on Git's `test-lib.sh`, sourced from the vendored
Git tree at `git/t/test-lib.sh`.  This provides:

- TAP-compatible output
- Test assertions (`test_expect_success`, `test_expect_failure`)
- Temporary directory management (`trash` directories)
- Color-coded pass/fail reporting

### `setup.sh`

All test scripts source `tests/setup.sh`, which provides:

```bash
# Core test helpers
prepare_tests()   # Create repos and config file
run_test()        # Execute a single test case
cgit_query()      # Invoke cgit with a query string
cgit_url()        # Invoke cgit with a virtual URL
strip_headers()   # Remove HTTP headers from CGI output
```

### Invoking cgit

Tests invoke cgit as a CGI binary by setting environment variables:

```bash
cgit_query()
{
    CGIT_CONFIG="$PWD/cgitrc" QUERY_STRING="$1" cgit
}

cgit_url()
{
    CGIT_CONFIG="$PWD/cgitrc" QUERY_STRING="url=$1" cgit
}
```

The `cgit` binary is on PATH (prepended by setup.sh).  The response includes
HTTP headers followed by HTML content.  `strip_headers()` removes the
headers for content-only assertions.

## Test Repository Setup

`setup_repos()` creates test repositories:

```bash
setup_repos()
{
    rm -rf cache
    mkdir -p cache
    mkrepo repos/foo 5           # 5 commits
    mkrepo repos/bar 50 commit-graph  # 50 commits with commit-graph
    mkrepo repos/foo+bar 10 testplus  # 10 commits + special chars
    mkrepo "repos/with space" 2  # repo with spaces in name
    mkrepo repos/filter 5 testplus   # for filter tests
}
```

### `mkrepo()`

```bash
mkrepo() {
    name=$1
    count=$2
    test_create_repo "$name"
    (
        cd "$name"
        n=1
        while test $n -le $count; do
            echo $n >file-$n
            git add file-$n
            git commit -m "commit $n"
            n=$(expr $n + 1)
        done
        case "$3" in
        testplus)
            echo "hello" >a+b
            git add a+b
            git commit -m "add a+b"
            git branch "1+2"
            ;;
        commit-graph)
            git commit-graph write
            ;;
        esac
    )
}
```

### Test Configuration

A `cgitrc` file is generated in the test directory with:

```ini
virtual-root=/
cache-root=$PWD/cache
cache-size=1021
snapshots=tar.gz tar.bz tar.lz tar.xz tar.zst zip
enable-log-filecount=1
enable-log-linecount=1
summary-log=5
summary-branches=5
summary-tags=5
clone-url=git://example.org/$CGIT_REPO_URL.git
enable-filter-overrides=1
root-coc=$PWD/site-coc.txt
root-cla=$PWD/site-cla.txt
root-homepage=https://projecttick.org
root-homepage-title=Project Tick
root-link=GitHub|https://github.com/example
root-link=GitLab|https://gitlab.com/example
root-link=Codeberg|https://codeberg.org/example

repo.url=foo
repo.path=$PWD/repos/foo/.git

repo.url=bar
repo.path=$PWD/repos/bar/.git
repo.desc=the bar repo

repo.url=foo+bar
repo.path=$PWD/repos/foo+bar/.git
repo.desc=the foo+bar repo
# ...
```

## Test Scripts

### Test File Naming

Tests follow the convention `tNNNN-description.sh`:

| Test | Description |
|------|-------------|
| `t0001-validate-git-versions.sh` | Verify Git version compatibility |
| `t0010-validate-html.sh` | Validate HTML output |
| `t0020-validate-cache.sh` | Test cache system |
| `t0101-index.sh` | Repository index page |
| `t0102-summary.sh` | Repository summary page |
| `t0103-log.sh` | Log view |
| `t0104-tree.sh` | Tree view |
| `t0105-commit.sh` | Commit view |
| `t0106-diff.sh` | Diff view |
| `t0107-snapshot.sh` | Snapshot downloads |
| `t0108-patch.sh` | Patch view |
| `t0109-gitconfig.sh` | Git config integration |
| `t0110-rawdiff.sh` | Raw diff output |
| `t0111-filter.sh` | Filter system |
| `t0112-coc.sh` | Code of Conduct page |
| `t0113-cla.sh` | CLA page |
| `t0114-root-homepage.sh` | Root homepage links |

### Number Ranges

| Range | Category |
|-------|----------|
| `t0001-t0099` | Infrastructure/validation tests |
| `t0100-t0199` | Feature tests |

## Running Tests

### All Tests

```bash
cd cgit/tests
make
```

The Makefile discovers all `t*.sh` files and runs them:

```makefile
T = $(wildcard t[0-9][0-9][0-9][0-9]-*.sh)

all: $(T)

$(T):
    @'$(SHELL_PATH_SQ)' $@ $(CGIT_TEST_OPTS)
```

### Individual Tests

```bash
# Run a single test
./t0101-index.sh

# With verbose output
./t0101-index.sh -v

# With Valgrind
./t0101-index.sh --valgrind
```

### Test Options

Options are passed via `CGIT_TEST_OPTS` or command-line arguments:

| Option | Description |
|--------|-------------|
| `-v`, `--verbose` | Show test details |
| `--valgrind` | Run cgit under Valgrind |
| `--debug` | Show shell trace |

### Valgrind Support

`setup.sh` intercepts the `--valgrind` flag and configures Valgrind
instrumentation via a wrapper script in `tests/valgrind/`:

```bash
if test -n "$cgit_valgrind"; then
    GIT_VALGRIND="$TEST_DIRECTORY/valgrind"
    CGIT_VALGRIND=$(cd ../valgrind && pwd)
    PATH="$CGIT_VALGRIND/bin:$PATH"
fi
```

## Test Patterns

### HTML Content Assertion

```bash
run_test 'repo index contains foo' '
    cgit_url "/" | strip_headers | grep -q "foo"
'
```

### HTTP Header Assertion

```bash
run_test 'content type is text/html' '
    cgit_url "/" | head -1 | grep -q "Content-Type: text/html"
'
```

### Snapshot Download

```bash
run_test 'snapshot is valid tar.gz' '
    cgit_url "/foo/snapshot/foo-master.tar.gz" | strip_headers | \
        gunzip | tar tf - >/dev/null
'
```

### Negative Assertion

```bash
run_test 'no 404 on valid repo' '
    ! cgit_url "/foo" | grep -q "404"
'
```

### Lua Filter Conditional

```bash
if [ $CGIT_HAS_LUA -eq 1 ]; then
    run_test 'lua filter works' '
        cgit_url "/filter-lua/about/" | strip_headers | grep -q "filtered"
    '
fi
```

## Test Filter Scripts

The `tests/filters/` directory contains simple filter scripts for testing:

### `dump.sh`

A passthrough filter that copies stdin to stdout, used to verify filter
invocation:

```bash
#!/bin/sh
cat
```

### `dump.lua`

Lua equivalent of the dump filter:

```lua
function filter_open(...)
end

function write(str)
    html(str)
end

function filter_close()
    return 0
end
```

## Cleanup

```bash
cd cgit/tests
make clean
```

Removes the `trash` directories created by tests.

## Writing New Tests

1. Create a new file `tNNNN-description.sh`
2. Source `setup.sh` and call `prepare_tests`:

```bash
#!/bin/sh
. ./setup.sh
prepare_tests "my new feature"

run_test 'description of test case' '
    cgit_url "/foo/my-page/" | strip_headers | grep -q "expected"
'
```

3. Make it executable: `chmod +x tNNNN-description.sh`
4. Run: `./tNNNN-description.sh -v`

## CI Integration

Tests are run as part of the CI pipeline.  The `ci/` directory contains
Nix-based CI configuration that builds cgit and runs the test suite in a
reproducible environment.
