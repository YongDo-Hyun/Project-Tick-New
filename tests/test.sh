#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EXPR_BIN=${EXPR_BIN:-"$ROOT/out/expr"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/expr-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

LC_ALL=C
LANG=C
export LC_ALL LANG

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_status() {
	name=$1
	expected=$2
	actual=$3

	[ "$expected" -eq "$actual" ] || fail "$name: expected exit $expected got $actual"
}

assert_file_match() {
	name=$1
	pattern=$2
	actual_file=$3

	if ! grep -Eq "$pattern" "$actual_file"; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- actual ---" >&2
		cat "$actual_file" >&2
		exit 1
	fi
}

assert_file_eq() {
	name=$1
	expected_text=$2
	actual_file=$3
	expected_file=$WORKDIR/expected.$$

	printf '%s' "$expected_text" > "$expected_file"
	if ! cmp -s "$expected_file" "$actual_file"; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- expected ---" >&2
		od -An -tx1 -v "$expected_file" >&2
		printf '%s\n' "--- actual ---" >&2
		od -An -tx1 -v "$actual_file" >&2
		exit 1
	fi
}

run_case() {
	name=$1
	expected_status=$2
	expected_stdout=$3
	expected_stderr=$4
	shift 4

	stdout_file=$WORKDIR/stdout
	stderr_file=$WORKDIR/stderr

	if "$EXPR_BIN" "$@" >"$stdout_file" 2>"$stderr_file"; then
		status=0
	else
		status=$?
	fi

	assert_status "$name status" "$expected_status" "$status"
	assert_file_eq "$name stdout" "$expected_stdout" "$stdout_file"
	assert_file_eq "$name stderr" "$expected_stderr" "$stderr_file"
}

run_case_match_stderr() {
	name=$1
	expected_status=$2
	expected_stdout=$3
	stderr_pattern=$4
	shift 4

	stdout_file=$WORKDIR/stdout
	stderr_file=$WORKDIR/stderr

	if "$EXPR_BIN" "$@" >"$stdout_file" 2>"$stderr_file"; then
		status=0
	else
		status=$?
	fi

	assert_status "$name status" "$expected_status" "$status"
	assert_file_eq "$name stdout" "$expected_stdout" "$stdout_file"
	assert_file_match "$name stderr" "$stderr_pattern" "$stderr_file"
}

derive_intmax_bounds() {
	power=1
	steps=0

	while next=$("$EXPR_BIN" "$power" "*" "2" 2>/dev/null); do
		power=$next
		steps=$((steps + 1))
		[ "$steps" -le 256 ] || fail "could not derive intmax bounds"
	done

	INTMAX_MAX_VALUE=$("$EXPR_BIN" "(" "$power" "-" "1" ")" "+" "$power")
	INTMAX_MIN_VALUE=$("$EXPR_BIN" "0" "-" "1" "-" "$INTMAX_MAX_VALUE")
	export INTMAX_MAX_VALUE INTMAX_MIN_VALUE
}

[ -x "$EXPR_BIN" ] || fail "missing binary: $EXPR_BIN"
derive_intmax_bounds

run_case "no arguments" 2 "" "usage: expr [-e] [--] expression
"
run_case "invalid option" 2 "" "usage: expr [-e] [--] expression
" "-x"
run_case "integer addition" 0 "7
" "" "3" "+" "4"
run_case "zero exit status" 1 "0
" "" "3" "-" "3"
run_case "parentheses precedence" 0 "21
" "" "(" "3" "+" "4" ")" "*" "3"
run_case "left associative subtraction" 0 "3
" "" "8" "-" "3" "-" "2"
run_case "left associative division" 0 "2
" "" "12" "/" "3" "/" "2"
run_case "division by zero" 2 "" "expr: division by zero
" "7" "/" "0"
run_case "remainder by zero" 2 "" "expr: division by zero
" "7" "%" "0"
run_case "type mismatch" 2 "" "expr: not a decimal number: 'abc'
" "abc" "+" "1"
run_case "too large operand" 2 "" "expr: operand too large: '999999999999999999999999999999999999'
" "999999999999999999999999999999999999" "+" "1"
run_case "addition overflow" 2 "" "expr: overflow
" "$INTMAX_MAX_VALUE" "+" "1"
run_case "subtraction overflow" 2 "" "expr: overflow
" "--" "$INTMAX_MIN_VALUE" "-" "1"
run_case "multiplication overflow" 2 "" "expr: overflow
" "$INTMAX_MAX_VALUE" "*" "2"
run_case "division overflow" 2 "" "expr: overflow
" "--" "$INTMAX_MIN_VALUE" "/" "-1"
run_case "string comparison" 0 "1
" "" "b" ">" "a"
run_case "numeric string comparison" 0 "1
" "" "--" "09" "=" "9"
run_case "strict plus stays string" 1 "0
" "" "--" "+7" "=" "7"
run_case "logical or keeps left value" 0 "left
" "" "left" "|" "right"
run_case "logical or returns right" 0 "fallback
" "" "0" "|" "fallback"
run_case "logical and keeps left value" 0 "left
" "" "left" "&" "right"
run_case "logical and returns zero" 1 "0
" "" "" "&" "right"
run_case "regex length" 0 "6
" "" "abcdef" ":" "abc.*"
run_case "regex capture" 0 "de
" "" "abcde" ":" "abc\\(..\\)"
run_case "regex anchor" 1 "0
" "" "abcde" ":" "bc"
run_case "regex capture miss" 1 "
" "" "abcde" ":" "zzz\\(..\\)"
run_case_match_stderr "regex syntax error" 2 "" '^expr: regular expression error: ' "abc" ":" "[["
run_case "strict negative needs --" 2 "" "usage: expr [-e] [--] expression
" "-1" "+" "2"
run_case "negative with --" 0 "1
" "" "--" "-1" "+" "2"
run_case "compat still needs -- for leading negative operand" 2 "" "usage: expr [-e] [--] expression
" "-e" "-1" "+" "2"
run_case "compat leading plus" 0 "1
" "" "-e" " +7" "=" "7"
run_case "compat empty string as zero" 0 "1
" "" "-e" "" "+" "1"
run_case "compat empty string compares as zero" 0 "1
" "" "-e" "" "=" "0"
run_case "syntax error" 2 "" "expr: syntax error
" "1" "+"
run_case "trailing token syntax error" 2 "" "expr: syntax error
" "1" "2"
run_case "missing closing parenthesis" 2 "" "expr: syntax error
" "(" "1" "+" "2"

stdout_file=$WORKDIR/write-fail.stdout
stderr_file=$WORKDIR/write-fail.stderr
if (
	exec 1>&-
	"$EXPR_BIN" "write-test"
) >"$stdout_file" 2>"$stderr_file"; then
	status=0
else
	status=$?
fi
assert_status "write failure status" 2 "$status"
assert_file_eq "write failure stdout" "" "$stdout_file"
case $(cat "$stderr_file") in
	"expr: write failed: "* ) ;;
	* ) fail "write failure stderr" ;;
esac

printf '%s\n' "PASS"
