#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
RM_BIN=${RM_BIN:-"$ROOT/out/rm"}
UNLINK_BIN=${UNLINK_BIN:-"$ROOT/out/unlink"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/rm-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
LAST_STATUS=0
LAST_STDOUT=
LAST_STDERR=
trap 'chmod -R u+rwx "$WORKDIR" 2>/dev/null || true; rm -rf "$WORKDIR"' EXIT INT TERM

export LC_ALL=C

RM_USAGE='usage: rm [-f | -i] [-dIPRrvWx] file ...'
UNLINK_USAGE='usage: unlink [--] file'

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

sort_lines() {
	printf '%s\n' "$1" | sort
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

run_with_input() {
	input=$1
	shift
	if printf '%b' "$input" | "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi
	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

[ -x "$RM_BIN" ] || fail "missing binary: $RM_BIN"
[ -x "$UNLINK_BIN" ] || fail "missing binary: $UNLINK_BIN"

run_capture "$RM_BIN"
assert_status "rm usage status" 2 "$LAST_STATUS"
assert_empty "rm usage stdout" "$LAST_STDOUT"
assert_eq "rm usage stderr" "$RM_USAGE" "$LAST_STDERR"

run_capture "$UNLINK_BIN"
assert_status "unlink usage status" 2 "$LAST_STATUS"
assert_empty "unlink usage stdout" "$LAST_STDOUT"
assert_eq "unlink usage stderr" "$UNLINK_USAGE" "$LAST_STDERR"

run_capture "$RM_BIN" -z
assert_status "invalid option status" 2 "$LAST_STATUS"
assert_empty "invalid option stdout" "$LAST_STDOUT"
assert_contains "invalid option usage" "$LAST_STDERR" "$RM_USAGE"

run_capture "$RM_BIN" -W target
assert_status "unsupported W status" 1 "$LAST_STATUS"
assert_empty "unsupported W stdout" "$LAST_STDOUT"
assert_contains "unsupported W stderr" "$LAST_STDERR" "-W is unsupported on Linux"

run_capture "$RM_BIN" -f
assert_status "force without operands status" 0 "$LAST_STATUS"
assert_empty "force without operands stdout" "$LAST_STDOUT"
assert_empty "force without operands stderr" "$LAST_STDERR"

printf 'payload\n' >"$WORKDIR/basic"
run_capture "$RM_BIN" "$WORKDIR/basic"
assert_status "basic remove status" 0 "$LAST_STATUS"
assert_empty "basic remove stdout" "$LAST_STDOUT"
assert_empty "basic remove stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/basic" ] || fail "basic file still exists"

run_capture "$RM_BIN" "$WORKDIR/missing"
assert_status "missing remove status" 1 "$LAST_STATUS"
assert_empty "missing remove stdout" "$LAST_STDOUT"
assert_contains "missing remove stderr" "$LAST_STDERR" "$WORKDIR/missing"

run_capture "$RM_BIN" -f "$WORKDIR/missing"
assert_status "force missing status" 0 "$LAST_STATUS"
assert_empty "force missing stdout" "$LAST_STDOUT"
assert_empty "force missing stderr" "$LAST_STDERR"

mkdir "$WORKDIR/dir"
run_capture "$RM_BIN" "$WORKDIR/dir"
assert_status "directory without d status" 1 "$LAST_STATUS"
assert_empty "directory without d stdout" "$LAST_STDOUT"
assert_contains "directory without d stderr" "$LAST_STDERR" "is a directory"
[ -d "$WORKDIR/dir" ] || fail "directory without d removed unexpectedly"

run_capture "$RM_BIN" -d "$WORKDIR/dir"
assert_status "directory with d status" 0 "$LAST_STATUS"
assert_empty "directory with d stdout" "$LAST_STDOUT"
assert_empty "directory with d stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/dir" ] || fail "directory with d still exists"

mkdir -p "$WORKDIR/tree/sub"
printf 'a\n' >"$WORKDIR/tree/file"
printf 'b\n' >"$WORKDIR/tree/sub/child"
run_capture "$RM_BIN" -rv "$WORKDIR/tree"
assert_status "recursive verbose status" 0 "$LAST_STATUS"
assert_eq "recursive verbose stdout" "$(sort_lines "$WORKDIR/tree/sub/child
$WORKDIR/tree/sub
$WORKDIR/tree/file
$WORKDIR/tree")" "$(sort_lines "$LAST_STDOUT")"
assert_empty "recursive verbose stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/tree" ] || fail "recursive tree still exists"

printf '1\n' >"$WORKDIR/one"
printf '2\n' >"$WORKDIR/two"
printf '3\n' >"$WORKDIR/three"
printf '4\n' >"$WORKDIR/four"
run_with_input 'n\n' "$RM_BIN" -I "$WORKDIR/one" "$WORKDIR/two" "$WORKDIR/three" "$WORKDIR/four"
assert_status "interactive once no status" 1 "$LAST_STATUS"
assert_empty "interactive once no stdout" "$LAST_STDOUT"
assert_contains "interactive once no prompt" "$LAST_STDERR" "remove 4 files?"
[ -e "$WORKDIR/one" ] || fail "interactive once no removed file"

run_with_input 'y\n' "$RM_BIN" -I "$WORKDIR/one" "$WORKDIR/two" "$WORKDIR/three" "$WORKDIR/four"
assert_status "interactive once yes status" 0 "$LAST_STATUS"
assert_empty "interactive once yes stdout" "$LAST_STDOUT"
assert_contains "interactive once yes prompt" "$LAST_STDERR" "remove 4 files?"
[ ! -e "$WORKDIR/one" ] || fail "interactive once yes kept file"

printf 'keep\n' >"$WORKDIR/ifile"
run_with_input 'n\n' "$RM_BIN" -i "$WORKDIR/ifile"
assert_status "interactive no status" 0 "$LAST_STATUS"
assert_empty "interactive no stdout" "$LAST_STDOUT"
assert_contains "interactive no prompt" "$LAST_STDERR" "remove $WORKDIR/ifile?"
[ -e "$WORKDIR/ifile" ] || fail "interactive no removed file"

run_with_input 'y\n' "$RM_BIN" -i "$WORKDIR/ifile"
assert_status "interactive yes status" 0 "$LAST_STATUS"
assert_empty "interactive yes stdout" "$LAST_STDOUT"
assert_contains "interactive yes prompt" "$LAST_STDERR" "remove $WORKDIR/ifile?"
[ ! -e "$WORKDIR/ifile" ] || fail "interactive yes kept file"

mkdir -p "$WORKDIR/idir/sub"
printf 'child\n' >"$WORKDIR/idir/sub/file"
run_with_input 'n\n' "$RM_BIN" -ri "$WORKDIR/idir"
assert_status "interactive recursive no status" 0 "$LAST_STATUS"
assert_empty "interactive recursive no stdout" "$LAST_STDOUT"
assert_contains "interactive recursive no prompt" "$LAST_STDERR" "descend into directory $WORKDIR/idir?"
[ -d "$WORKDIR/idir" ] || fail "interactive recursive no removed directory"

run_with_input 'y\ny\ny\ny\ny\n' "$RM_BIN" -ri "$WORKDIR/idir"
assert_status "interactive recursive yes status" 0 "$LAST_STATUS"
assert_empty "interactive recursive yes stdout" "$LAST_STDOUT"
assert_contains "interactive recursive yes prompt 1" "$LAST_STDERR" "descend into directory $WORKDIR/idir?"
assert_contains "interactive recursive yes prompt 2" "$LAST_STDERR" "descend into directory $WORKDIR/idir/sub?"
assert_contains "interactive recursive yes prompt 3" "$LAST_STDERR" "remove $WORKDIR/idir/sub/file?"
assert_contains "interactive recursive yes prompt 4" "$LAST_STDERR" "remove directory $WORKDIR/idir/sub?"
[ -n "$LAST_STDERR" ] || fail "interactive recursive yes missing prompts"
[ ! -e "$WORKDIR/idir" ] || fail "interactive recursive yes kept directory"

run_capture "$RM_BIN" /
assert_status "slash operand status" 1 "$LAST_STATUS"
assert_empty "slash operand stdout" "$LAST_STDOUT"
assert_contains "slash operand stderr" "$LAST_STDERR" "\"/\" may not be removed"

mkdir -p "$WORKDIR/dots"
(
	cd "$WORKDIR/dots"
	run_capture "$RM_BIN" .
	assert_status "dot operand status" 1 "$LAST_STATUS"
	assert_empty "dot operand stdout" "$LAST_STDOUT"
	assert_contains "dot operand stderr" "$LAST_STDERR" "\".\" and \"..\" may not be removed"

	run_capture "$RM_BIN" sub/..
	assert_status "dotdot operand status" 1 "$LAST_STATUS"
	assert_empty "dotdot operand stdout" "$LAST_STDOUT"
	assert_contains "dotdot operand stderr" "$LAST_STDERR" "\".\" and \"..\" may not be removed"
)

printf 'dash\n' >"$WORKDIR/-dash"
run_capture "$RM_BIN" -- "$WORKDIR/-dash"
assert_status "dash operand status" 0 "$LAST_STATUS"
assert_empty "dash operand stdout" "$LAST_STDOUT"
assert_empty "dash operand stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/-dash" ] || fail "dash operand still exists"

printf 'noop\n' >"$WORKDIR/noop"
run_capture "$RM_BIN" -P "$WORKDIR/noop"
assert_status "P noop status" 0 "$LAST_STATUS"
assert_empty "P noop stdout" "$LAST_STDOUT"
assert_empty "P noop stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/noop" ] || fail "P noop failed"

mkdir -p "$WORKDIR/xdev/dir"
printf 'x\n' >"$WORKDIR/xdev/dir/file"
run_capture "$RM_BIN" -rx "$WORKDIR/xdev"
assert_status "x option status" 0 "$LAST_STATUS"
assert_empty "x option stdout" "$LAST_STDOUT"
assert_empty "x option stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/xdev" ] || fail "x option failed"

printf 'unlink\n' >"$WORKDIR/unlink-file"
run_capture "$UNLINK_BIN" -- "$WORKDIR/unlink-file"
assert_status "unlink success status" 0 "$LAST_STATUS"
assert_empty "unlink success stdout" "$LAST_STDOUT"
assert_empty "unlink success stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/unlink-file" ] || fail "unlink file still exists"

mkdir "$WORKDIR/unlink-dir"
run_capture "$UNLINK_BIN" "$WORKDIR/unlink-dir"
assert_status "unlink dir status" 1 "$LAST_STATUS"
assert_empty "unlink dir stdout" "$LAST_STDOUT"
assert_contains "unlink dir stderr" "$LAST_STDERR" "is a directory"
[ -d "$WORKDIR/unlink-dir" ] || fail "unlink dir removed unexpectedly"

printf '%s\n' "PASS"
