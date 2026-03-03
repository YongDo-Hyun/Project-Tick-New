#!/bin/sh
set -eu

export LC_ALL=C
export TZ=UTC

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
REALPATH_BIN="${REALPATH_BIN:-$ROOT/out/realpath}"

TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/realpath-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf 'FAIL: %s\n' "$1" >&2
	exit 1
}

pass() {
	printf 'PASS: %s\n' "$1"
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

assert_status() {
	name=$1
	expected=$2
	actual=$3
	if [ "$expected" -ne "$actual" ]; then
		printf 'FAIL: %s\n' "$name" >&2
		printf '  expected status: %s\n' "$expected" >&2
		printf '  actual status:   %s\n' "$actual" >&2
		exit 1
	fi
}

assert_eq() {
	name=$1
	expected=$2
	actual=$3
	if [ "$expected" != "$actual" ]; then
		printf 'FAIL: %s\n' "$name" >&2
		printf '  expected: %s\n' "$expected" >&2
		printf '  actual:   %s\n' "$actual" >&2
		exit 1
	fi
}

assert_empty() {
	name=$1
	text=$2
	if [ -n "$text" ]; then
		printf 'FAIL: %s\n' "$name" >&2
		printf '  expected empty\n' >&2
		printf '  actual: %s\n' "$text" >&2
		exit 1
	fi
}

assert_contains() {
	name=$1
	text=$2
	pattern=$3
	case $text in
		*"$pattern"*) ;;
		*) fail "$name: missing '$pattern' in '$text'" ;;
	esac
}

assert_contains_one_of() {
	name=$1
	text=$2
	first=$3
	second=$4
	case $text in
		*"$first"*|*"$second"*) ;;
		*) fail "$name: missing '$first' or '$second' in '$text'" ;;
	esac
}

[ -x "$REALPATH_BIN" ] || fail "missing or non-executable binary: $REALPATH_BIN"

BASE="$WORKDIR/tree"
mkdir -p "$BASE/dir/subdir"
printf 'payload\n' >"$BASE/dir/subdir/file.txt"
ln -s dir "$BASE/linkdir"
ln -s dir/subdir/file.txt "$BASE/filelink"
MISSING="$BASE/does-not-exist"
LEADING_DASH="$BASE/-dash-target"
mkdir -p "$LEADING_DASH"

EXPECTED_FILE="$BASE/dir/subdir/file.txt"
EXPECTED_DIR="$BASE/dir"

run_capture "$REALPATH_BIN" "$BASE/filelink"
assert_status "basic symlink resolution status" 0 "$LAST_STATUS"
assert_eq "basic symlink resolution stdout" "$EXPECTED_FILE" "$LAST_STDOUT"
assert_empty "basic symlink resolution stderr" "$LAST_STDERR"
pass "resolves symlink operand"

(
	cd "$BASE/linkdir/subdir"
	run_capture "$REALPATH_BIN"
	assert_status "default operand status" 0 "$LAST_STATUS"
	assert_eq "default operand stdout exact" "$BASE/dir/subdir" "$LAST_STDOUT"
	assert_empty "default operand stderr" "$LAST_STDERR"
)
pass "defaults to current working directory"

run_capture "$REALPATH_BIN" "$BASE/linkdir" "$MISSING" "$BASE/filelink"
assert_status "mixed operands status" 1 "$LAST_STATUS"
assert_eq "mixed operands stdout" "$EXPECTED_DIR
$EXPECTED_FILE" "$LAST_STDOUT"
assert_contains "mixed operands stderr has missing path" "$LAST_STDERR" "$MISSING"
assert_contains "mixed operands stderr has enoent" "$LAST_STDERR" "No such file or directory"
pass "mixed success and failure keeps successful output and exits non-zero"

run_capture "$REALPATH_BIN" -q "$MISSING" "$BASE/filelink"
assert_status "quiet failure status" 1 "$LAST_STATUS"
assert_eq "quiet failure stdout" "$EXPECTED_FILE" "$LAST_STDOUT"
assert_empty "quiet failure stderr" "$LAST_STDERR"
pass "quiet mode suppresses path-resolution diagnostics only"

run_capture "$REALPATH_BIN" -- -dash-target
assert_status "double-dash status" 1 "$LAST_STATUS"
assert_empty "double-dash stdout" "$LAST_STDOUT"
assert_contains "double-dash stderr path" "$LAST_STDERR" "-dash-target"
pass "double-dash ends option parsing"

run_capture "$REALPATH_BIN" "$LEADING_DASH"
assert_status "leading-dash operand status" 0 "$LAST_STATUS"
assert_eq "leading-dash operand stdout" "$LEADING_DASH" "$LAST_STDOUT"
assert_empty "leading-dash operand stderr" "$LAST_STDERR"
pass "leading dash operand works when not parsed as an option"

run_capture "$REALPATH_BIN" -x
assert_status "invalid option status" 1 "$LAST_STATUS"
assert_empty "invalid option stdout" "$LAST_STDOUT"
assert_contains "invalid option stderr mentions option" "$LAST_STDERR" "illegal option -- x"
assert_contains "invalid option stderr mentions usage" "$LAST_STDERR" "usage: realpath [-q] [path ...]"
pass "invalid option is rejected with usage"

run_capture "$REALPATH_BIN" -q -x
assert_status "quiet invalid option status" 1 "$LAST_STATUS"
assert_empty "quiet invalid option stdout" "$LAST_STDOUT"
assert_contains "quiet invalid option still warns" "$LAST_STDERR" "illegal option -- x"
pass "quiet mode does not suppress parse errors"

run_capture "$REALPATH_BIN" "$BASE/dir/../linkdir/./subdir/../subdir/file.txt"
assert_status "dot and dotdot normalization status" 0 "$LAST_STATUS"
assert_eq "dot and dotdot normalization stdout" "$EXPECTED_FILE" "$LAST_STDOUT"
assert_empty "dot and dotdot normalization stderr" "$LAST_STDERR"
pass "normalizes dot and dotdot components"

run_capture "$REALPATH_BIN" "$BASE/dir/subdir/file.txt/"
assert_status "nondirectory trailing slash status" 1 "$LAST_STATUS"
assert_empty "nondirectory trailing slash stdout" "$LAST_STDOUT"
assert_contains "nondirectory trailing slash stderr path" "$LAST_STDERR" "$BASE/dir/subdir/file.txt/"
pass "reports invalid trailing slash on nondirectory"

LOOPDIR="$BASE/loop"
mkdir -p "$LOOPDIR"
ln -s b "$LOOPDIR/a"
ln -s a "$LOOPDIR/b"

run_capture "$REALPATH_BIN" "$LOOPDIR/a"
assert_status "symlink loop status" 1 "$LAST_STATUS"
assert_empty "symlink loop stdout" "$LAST_STDOUT"
assert_contains "symlink loop stderr path" "$LAST_STDERR" "$LOOPDIR/a"
assert_contains_one_of "symlink loop stderr errno" "$LAST_STDERR" \
	"Too many levels of symbolic links" "Symbolic link loop"
pass "symlink loop reports ELOOP"

CHAIN_DIR="$BASE/chain"
mkdir -p "$CHAIN_DIR"
printf 'end\n' >"$CHAIN_DIR/target"
i=45
while [ "$i" -gt 1 ]; do
	next=$((i - 1))
	ln -s "link$i" "$CHAIN_DIR/link$next"
	i=$next
done
ln -s target "$CHAIN_DIR/link45"

run_capture "$REALPATH_BIN" "$CHAIN_DIR/link1"
assert_status "deep symlink chain status" 1 "$LAST_STATUS"
assert_empty "deep symlink chain stdout" "$LAST_STDOUT"
assert_contains "deep symlink chain stderr path" "$LAST_STDERR" "$CHAIN_DIR/link1"
assert_contains_one_of "deep symlink chain stderr errno" "$LAST_STDERR" \
	"Too many levels of symbolic links" "Symbolic link loop"
pass "deep symlink chain hits Linux ELOOP boundary"

BROKEN_LINK="$BASE/broken-link"
ln -s does-not-exist "$BROKEN_LINK"

run_capture "$REALPATH_BIN" "$BROKEN_LINK"
assert_status "broken symlink status" 1 "$LAST_STATUS"
assert_empty "broken symlink stdout" "$LAST_STDOUT"
assert_contains "broken symlink stderr path" "$LAST_STDERR" "$BROKEN_LINK"
assert_contains "broken symlink stderr errno" "$LAST_STDERR" "No such file or directory"
pass "dangling symlink fails with ENOENT"

FILE_SYMLINK="$BASE/file-symlink"
ln -s "$BASE/dir/subdir/file.txt" "$FILE_SYMLINK"

run_capture "$REALPATH_BIN" "$FILE_SYMLINK/"
assert_status "symlink-to-file trailing slash status" 1 "$LAST_STATUS"
assert_empty "symlink-to-file trailing slash stdout" "$LAST_STDOUT"
assert_contains "symlink-to-file trailing slash stderr path" "$LAST_STDERR" "$FILE_SYMLINK/"
assert_contains "symlink-to-file trailing slash errno" "$LAST_STDERR" "Not a directory"
pass "trailing slash on symlink to file reports ENOTDIR"

(
	cd /
	run_capture "$REALPATH_BIN" "../../.."
	assert_status "root boundary status" 0 "$LAST_STATUS"
	assert_eq "root boundary stdout" "/" "$LAST_STDOUT"
	assert_empty "root boundary stderr" "$LAST_STDERR"
)
pass "relative traversal above root clamps to root"

run_capture "$REALPATH_BIN" ""
assert_status "empty operand status" 1 "$LAST_STATUS"
assert_empty "empty operand stdout" "$LAST_STDOUT"
assert_contains "empty operand stderr path" "$LAST_STDERR" "realpath: :"
pass "rejects empty operand"

if [ "$(id -u)" -eq 0 ]; then
	printf 'SKIP: permission denied test (running as root)\n' >&2
else
	SECRET_DIR="$BASE/secret"
	mkdir -p "$SECRET_DIR/inner"
	printf 'secret\n' >"$SECRET_DIR/inner/file"
	chmod 000 "$SECRET_DIR"
	run_capture "$REALPATH_BIN" "$SECRET_DIR/inner/file"
	chmod 700 "$SECRET_DIR"
	assert_status "permission denied status" 1 "$LAST_STATUS"
	assert_empty "permission denied stdout" "$LAST_STDOUT"
	assert_contains "permission denied stderr path" "$LAST_STDERR" "$SECRET_DIR/inner/file"
	assert_contains "permission denied stderr errno" "$LAST_STDERR" "Permission denied"
	pass "permission denied reports EACCES"
fi

LONG_ROOT="$WORKDIR/long-tree"
LONG_PATH_FILE="$WORKDIR/long-path"
mkdir -p "$LONG_ROOT"
if (
	cd "$LONG_ROOT"
	current=$LONG_ROOT
	i=0
	while [ "$i" -lt 90 ]; do
		part=$(printf 'd%03d_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx' "$i")
		mkdir "$part"
		cd "$part"
		current=$current/$part
		i=$((i + 1))
	done
	printf '%s' "$current" >"$LONG_PATH_FILE"
); then
	LONG_PATH=$(cat "$LONG_PATH_FILE")
	long_length=$(printf '%s' "$LONG_PATH" | wc -c | tr -d ' ')
	run_capture "$REALPATH_BIN" "$LONG_PATH"
	if [ "$LAST_STATUS" -eq 0 ]; then
		assert_eq "long path stdout" "$LONG_PATH" "$LAST_STDOUT"
		assert_empty "long path stderr" "$LAST_STDERR"
		pass "very long path resolves without truncation"
	elif [ "$LAST_STATUS" -eq 1 ]; then
		assert_empty "long path stdout on failure" "$LAST_STDOUT"
		assert_contains "long path stderr path" "$LAST_STDERR" "$LONG_ROOT"
		assert_contains_one_of "long path errno" "$LAST_STDERR" \
			"File name too long" "Filename too long"
		pass "very long path fails explicitly at Linux pathname limit"
	else
		fail "long path unexpected exit status: $LAST_STATUS"
	fi
	if [ "$long_length" -lt 4096 ]; then
		fail "long path length too short: $long_length"
	fi
else
	printf 'SKIP: long path setup failed\n' >&2
fi

(
	RESULT_FILE="$WORKDIR/pipe-status"
	ERR_FILE="$WORKDIR/pipe-stderr"

	sh -c '
		trap "" PIPE
		"$1" "$2" 2>"$3"
		echo $? >"$4"
	' -- "$REALPATH_BIN" "$BASE/filelink" "$ERR_FILE" "$RESULT_FILE" | true || true

	i=0
	while [ "$i" -lt 20 ] && [ ! -s "$RESULT_FILE" ]; do
		sleep 0.1
		i=$((i + 1))
	done

	if [ ! -s "$RESULT_FILE" ]; then
		printf 'SKIP: stdout failure test (no status file)\n' >&2
	else
		pipe_status=$(cat "$RESULT_FILE")
		pipe_stderr=$(cat "$ERR_FILE" 2>/dev/null || true)
		if [ "$pipe_status" -eq 0 ]; then
			printf 'SKIP: stdout failure test (pipe consumer timing)\n' >&2
		else
			assert_contains "stdout failure stderr" "$pipe_stderr" "stdout"
			pass "stdout failure returns non-zero with diagnostic"
		fi
	fi
)
