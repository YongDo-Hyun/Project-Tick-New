#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
MKDIR_BIN=${MKDIR_BIN:-"$ROOT/out/mkdir"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/mkdir-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
LAST_STATUS=0
LAST_STDOUT=
LAST_STDERR=
trap 'chmod -R u+rwx "$WORKDIR" 2>/dev/null || true; rm -rf "$WORKDIR"' EXIT INT TERM

export LC_ALL=C

USAGE_TEXT='usage: mkdir [-pv] [-m mode] directory_name ...'

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_eq() {
	name=$1
	expected=$2
	actual=$3
	if [ "$expected" != "$actual" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- expected ---" >&2
		printf '%s' "$expected" >&2
		printf '\n%s\n' "--- actual ---" >&2
		printf '%s' "$actual" >&2
		printf '\n' >&2
		exit 1
	fi
}

assert_contains() {
	name=$1
	text=$2
	pattern=$3
	case $text in
		*"$pattern"*) ;;
		*) fail "$name" ;;
	esac
}

assert_empty() {
	name=$1
	text=$2
	if [ -n "$text" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- expected empty ---" >&2
		printf '%s\n' "--- actual ---" >&2
		printf '%s' "$text" >&2
		printf '\n' >&2
		exit 1
	fi
}

assert_status() {
	name=$1
	expected=$2
	actual=$3
	if [ "$expected" -ne "$actual" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "expected status: $expected" >&2
		printf '%s\n' "actual status: $actual" >&2
		exit 1
	fi
}

assert_mode() {
	expected=$1
	path=$2
	actual=$(stat -c '%a' "$path")
	[ "$actual" = "$expected" ] || fail "$path mode expected $expected got $actual"
}

run_capture() {
	if "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi
	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

[ -x "$MKDIR_BIN" ] || fail "missing binary: $MKDIR_BIN"

run_capture "$MKDIR_BIN"
assert_status "usage status" 2 "$LAST_STATUS"
assert_empty "usage stdout" "$LAST_STDOUT"
assert_eq "usage stderr" "$USAGE_TEXT" "$LAST_STDERR"

run_capture "$MKDIR_BIN" -m
assert_status "missing mode argument status" 2 "$LAST_STATUS"
assert_empty "missing mode argument stdout" "$LAST_STDOUT"
assert_contains "missing mode argument stderr has usage" "$LAST_STDERR" "$USAGE_TEXT"

run_capture "$MKDIR_BIN" -z
assert_status "invalid option status" 2 "$LAST_STATUS"
assert_empty "invalid option stdout" "$LAST_STDOUT"
assert_contains "invalid option stderr has usage" "$LAST_STDERR" "$USAGE_TEXT"

run_capture "$MKDIR_BIN" -m invalid "$WORKDIR/invalid"
assert_status "invalid mode status" 1 "$LAST_STATUS"
assert_empty "invalid mode stdout" "$LAST_STDOUT"
assert_contains "invalid mode stderr" "$LAST_STDERR" "invalid file mode: invalid"

run_capture "$MKDIR_BIN" "$WORKDIR/basic"
assert_status "basic status" 0 "$LAST_STATUS"
assert_empty "basic stdout" "$LAST_STDOUT"
assert_empty "basic stderr" "$LAST_STDERR"
[ -d "$WORKDIR/basic" ] || fail "basic directory missing"

run_capture "$MKDIR_BIN" "$WORKDIR/basic"
assert_status "existing dir status" 1 "$LAST_STATUS"
assert_empty "existing dir stdout" "$LAST_STDOUT"
assert_contains "existing dir stderr" "$LAST_STDERR" "$WORKDIR/basic"

mode_dir="$WORKDIR/mode-explicit"
run_capture sh -c 'umask 077; "$1" -m 755 "$2"' sh "$MKDIR_BIN" "$mode_dir"
assert_status "explicit numeric mode status" 0 "$LAST_STATUS"
assert_empty "explicit numeric stdout" "$LAST_STDOUT"
assert_empty "explicit numeric stderr" "$LAST_STDERR"
assert_mode 755 "$mode_dir"

symbolic_dir="$WORKDIR/mode-symbolic"
run_capture sh -c 'umask 077; "$1" -m u=rwx,go=rx "$2"' sh "$MKDIR_BIN" "$symbolic_dir"
assert_status "explicit symbolic mode status" 0 "$LAST_STATUS"
assert_empty "explicit symbolic stdout" "$LAST_STDOUT"
assert_empty "explicit symbolic stderr" "$LAST_STDERR"
assert_mode 755 "$symbolic_dir"

relative_symbolic_dir="$WORKDIR/mode-relative-symbolic"
run_capture sh -c 'umask 077; "$1" -m -w "$2"' sh "$MKDIR_BIN" "$relative_symbolic_dir"
assert_status "relative symbolic mode status" 0 "$LAST_STATUS"
assert_empty "relative symbolic stdout" "$LAST_STDOUT"
assert_empty "relative symbolic stderr" "$LAST_STDERR"
assert_mode 555 "$relative_symbolic_dir"

special_mode_dir="$WORKDIR/mode-special"
run_capture "$MKDIR_BIN" -m 1755 "$special_mode_dir"
assert_status "special mode status" 0 "$LAST_STATUS"
assert_empty "special mode stdout" "$LAST_STDOUT"
assert_empty "special mode stderr" "$LAST_STDERR"
[ "$(stat -c '%a' "$special_mode_dir")" = "1755" ] || fail "special mode bits missing"

verbose_dir="$WORKDIR/verbose/a/b"
run_capture "$MKDIR_BIN" -pv "$verbose_dir"
assert_status "verbose recursive status" 0 "$LAST_STATUS"
assert_eq "verbose recursive stdout" "$WORKDIR/verbose
$WORKDIR/verbose/a
$WORKDIR/verbose/a/b" "$LAST_STDOUT"
assert_empty "verbose recursive stderr" "$LAST_STDERR"

restricted_root="$WORKDIR/restrict"
run_capture sh -c 'umask 0777; "$1" -p "$2/one/two"' sh "$MKDIR_BIN" "$restricted_root"
assert_status "restrictive umask status" 0 "$LAST_STATUS"
assert_empty "restrictive umask stdout" "$LAST_STDOUT"
assert_empty "restrictive umask stderr" "$LAST_STDERR"
assert_mode 300 "$restricted_root/one"
assert_mode 0 "$restricted_root/one/two"

mkdir "$WORKDIR/existing"
run_capture "$MKDIR_BIN" -p "$WORKDIR/existing"
assert_status "existing with -p status" 0 "$LAST_STATUS"
assert_empty "existing with -p stdout" "$LAST_STDOUT"
assert_empty "existing with -p stderr" "$LAST_STDERR"

mkdir -p "$WORKDIR/verbose-existing/tree"
run_capture "$MKDIR_BIN" -pv "$WORKDIR/verbose-existing/tree/new/leaf"
assert_status "verbose skips existing status" 0 "$LAST_STATUS"
assert_eq "verbose skips existing stdout" "$WORKDIR/verbose-existing/tree/new
$WORKDIR/verbose-existing/tree/new/leaf" "$LAST_STDOUT"
assert_empty "verbose skips existing stderr" "$LAST_STDERR"

run_capture "$MKDIR_BIN" -p "$WORKDIR/slashes//deep///leaf///"
assert_status "redundant slashes status" 0 "$LAST_STATUS"
assert_empty "redundant slashes stdout" "$LAST_STDOUT"
assert_empty "redundant slashes stderr" "$LAST_STDERR"
[ -d "$WORKDIR/slashes/deep/leaf" ] || fail "redundant slash path missing"

printf 'not-a-dir\n' >"$WORKDIR/file"
run_capture "$MKDIR_BIN" -p "$WORKDIR/file/child"
assert_status "non-directory component status" 1 "$LAST_STATUS"
assert_empty "non-directory component stdout" "$LAST_STDOUT"
assert_contains "non-directory component stderr" "$LAST_STDERR" "$WORKDIR/file"

run_capture "$MKDIR_BIN" "$WORKDIR/missing/child"
assert_status "missing parent status" 1 "$LAST_STATUS"
assert_empty "missing parent stdout" "$LAST_STDOUT"
assert_contains "missing parent stderr" "$LAST_STDERR" "$WORKDIR/missing/child"

multi_one="$WORKDIR/multi-one"
multi_two_parent="$WORKDIR/multi-missing"
run_capture "$MKDIR_BIN" "$multi_one" "$multi_two_parent/child"
assert_status "multi operand partial failure status" 1 "$LAST_STATUS"
assert_empty "multi operand partial failure stdout" "$LAST_STDOUT"
assert_contains "multi operand partial failure stderr" "$LAST_STDERR" "$multi_two_parent/child"
[ -d "$multi_one" ] || fail "multi operand did not create successful directory"

printf '%s\n' "PASS"
