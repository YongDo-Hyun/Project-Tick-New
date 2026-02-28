#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DATE_BIN=${DATE_BIN:-"$ROOT/out/date"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/date-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_eq() {
	expected=$1
	actual=$2
	message=$3
	[ "$actual" = "$expected" ] || fail "$message: expected [$expected] got [$actual]"
}

assert_match() {
	pattern=$1
	text=$2
	message=$3
	printf '%s\n' "$text" | grep -Eq "$pattern" || fail "$message"
}

run_ok() {
	"$DATE_BIN" "$@"
}

run_err() {
	"$DATE_BIN" "$@" 2>&1 || true
}

[ -x "$DATE_BIN" ] || fail "missing binary: $DATE_BIN"

export LC_ALL=C
export TZ=UTC0

usage_output=$(run_err -x)
assert_match '^usage: date ' "$usage_output" "usage output missing"

unsupported_output=$(run_err -n)
assert_eq "date: option -n is not supported on Linux" "$unsupported_output" "unsupported -n check failed"

default_output=$(run_ok -u -r 3222243)
assert_eq "Sat Feb  7 07:04:03 UTC 1970" "$default_output" "default %+ format mismatch"

rfc_output=$(run_ok -u -R -r 3222243)
assert_eq "Sat, 07 Feb 1970 07:04:03 +0000" "$rfc_output" "RFC 2822 output mismatch"

iso_seconds=$(run_ok -u -r 1005600000 -Iseconds)
assert_eq "2001-11-12T21:20:00+00:00" "$iso_seconds" "ISO-8601 seconds output mismatch"

iso_ns=$(run_ok -u -r 3222243 -Ins)
assert_eq "1970-02-07T07:04:03,000000000+00:00" "$iso_ns" "ISO-8601 nanosecond output mismatch"

format_ns=$(run_ok -u -r 3222243 +%s.%N)
assert_eq "3222243.000000000" "$format_ns" "nanosecond format mismatch"

format_width=$(run_ok -u -r 3222243 +%3N)
assert_eq "000" "$format_width" "width-limited %N mismatch"

strict_parse=$(run_ok -j -u -f %Y-%m-%d 2001-11-12 +%F)
assert_eq "2001-11-12" "$strict_parse" "formatted parse mismatch"

legacy_parse=$(run_ok -j -u 200111122120.59 "+%F %T")
assert_eq "2001-11-12 21:20:59" "$legacy_parse" "legacy numeric parse mismatch"

vary_month=$(run_ok -j -u -r 1612051200 -v+1m +%F)
assert_eq "2021-02-28" "$vary_month" "month clamp adjustment mismatch"

zone_output=$(run_ok -r 0 -z UTC-2 +%Y-%m-%dT%H:%M:%S%z)
assert_eq "1970-01-01T02:00:00+0200" "$zone_output" "output zone mismatch"

touch -t 202001010102.03 "$WORKDIR/reference"
file_output=$(run_ok -u -r "$WORKDIR/reference" +%s)
assert_eq "1577840523" "$file_output" "file reference timestamp mismatch"

bad_format=$(run_err -j -f %Y-%m-%d 2001-11-12junk +%F)
assert_eq "date: input did not fully match format '%Y-%m-%d': 2001-11-12junk" "$bad_format" "strict format rejection missing"

bad_iso=$(run_err -Ibogus)
assert_eq "date: invalid argument 'bogus' for -I" "$bad_iso" "invalid -I rejection missing"

bad_reference=$(run_err -r "$WORKDIR/missing")
assert_match "^date: $WORKDIR/missing: " "$bad_reference" "missing reference error missing"

bad_operands=$(run_err 200101010101 200201010101)
assert_match '^usage: date ' "$bad_operands" "too many operands not rejected"

printf '%s\n' "PASS"
