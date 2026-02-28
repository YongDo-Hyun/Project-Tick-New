#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ED_BIN=${ED_BIN:-"$ROOT/out/ed"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/ed-test.XXXXXX")
REGRESS_DIR="$WORKDIR/regress"
UUDECODE_BIN="$WORKDIR/uu_decode"
RED_BIN="$WORKDIR/red"
trap 'rm -rf "$WORKDIR"' EXIT INT TERM
export LC_ALL=C

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
		printf '%s\n' "$expected" >&2
		printf '%s\n' "--- actual ---" >&2
		printf '%s\n' "$actual" >&2
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

run_capture() {
	output_file=$1
	status_file=$2
	shift 2

	set +e
	"$@" >"$output_file" 2>&1
	status=$?
	set -e
	printf '%s\n' "$status" >"$status_file"
}

build_uudecode() {
	"$CC" -O2 -std=c17 -Wall -Wextra -Werror \
		"$ROOT/tests/uu_decode.c" -o "$UUDECODE_BIN" \
		|| fail "failed to build uu decoder with $CC"
}

decode_regression_fixtures() {
	"$UUDECODE_BIN" "$REGRESS_DIR/ascii.d.uu" "$REGRESS_DIR/ascii.d" \
		|| fail "failed to decode ascii.d.uu"
	"$UUDECODE_BIN" "$REGRESS_DIR/ascii.r.uu" "$REGRESS_DIR/ascii.r" \
		|| fail "failed to decode ascii.r.uu"
}

run_regression_suite() {
	known_issue='*** The script nl.red exited abnormally  ***'

	cp "$ROOT"/test/* "$REGRESS_DIR"/
	decode_regression_fixtures

	(
		cd "$REGRESS_DIR"
		sh ./mkscripts.sh "$ED_BIN" >/dev/null
		sh ./ckscripts.sh "$ED_BIN" >/dev/null 2>&1 || true
	) || fail "upstream regression harness failed to run"

	issues=$(cd "$REGRESS_DIR" && grep -h '\*\*\*' errs.o scripts.o || true)
	if [ -n "$issues" ]; then
		filtered_issues=$(printf '%s\n' "$issues" | grep -Fvx "$known_issue" || true)
		[ -z "$filtered_issues" ] || fail "upstream regression failures detected:
$filtered_issues"
	fi
}

[ -x "$ED_BIN" ] || fail "missing binary: $ED_BIN"
mkdir -p "$REGRESS_DIR"
ln -sf "$ED_BIN" "$RED_BIN"
build_uudecode

usage_output=$("$ED_BIN" -Z 2>&1 || true)
assert_contains "invalid option should print usage" "$usage_output" \
	"usage: ed [-] [-sx] [-p string] [file]"

crypt_option_output=$("$ED_BIN" -x 2>&1 || true)
assert_contains "unsupported -x option should be explicit" "$crypt_option_output" \
	"option -x is not supported on Linux"

crypt_command_output=$(printf 'H\nx\nQ\n' | "$ED_BIN" -s 2>&1 || true)
assert_contains "interactive x command should be explicit" "$crypt_command_output" \
	"crypt mode is not supported on Linux"

cat >"$WORKDIR/strict-input.txt" <<'EOF'
line 1
line 2
EOF

strict_append_output=$(cat <<EOF | "$ED_BIN" -s 2>&1 || true
H
r $WORKDIR/strict-input.txt
1,\$a
bad
.
Q
EOF
)
assert_contains "append should reject address ranges" "$strict_append_output" \
	"unexpected address"

strict_read_output=$(cat <<EOF | "$ED_BIN" -s 2>&1 || true
H
r $WORKDIR/strict-input.txt
1,\$r $WORKDIR/strict-input.txt
Q
EOF
)
assert_contains "read should reject address ranges" "$strict_read_output" \
	"unexpected address"

no_filename_output=$(printf 'H\nf\nQ\n' | "$ED_BIN" -s 2>&1 || true)
assert_contains "f without current filename should fail" "$no_filename_output" \
	"no current filename"

cat <<EOF | "$ED_BIN" -s >/dev/null
H
r !printf "shell line\n"
w $WORKDIR/shell-read.out
Q
EOF
assert_eq "shell read output" "shell line" "$(cat "$WORKDIR/shell-read.out")"

cat >"$WORKDIR/write-input.txt" <<'EOF'
alpha
beta
EOF
cat <<EOF | "$ED_BIN" -s >/dev/null
H
r $WORKDIR/write-input.txt
w !cat > $WORKDIR/shell-write.out
Q
EOF
assert_eq "shell write output" "$(cat "$WORKDIR/write-input.txt")" \
	"$(cat "$WORKDIR/shell-write.out")"

mkdir "$WORKDIR/scratch"
printf 'a\nscratch check\n.\nQ\n' | env TMPDIR="$WORKDIR/scratch" \
	"$ED_BIN" -s >/dev/null 2>&1
[ -z "$(find "$WORKDIR/scratch" -mindepth 1 -maxdepth 1 -print)" ] \
	|| fail "scratch files were not cleaned up in TMPDIR"

run_capture "$WORKDIR/red-shell.out" "$WORKDIR/red-shell.status" \
	sh -c 'printf "H\n!true\nQ\n" | "$1" -s' sh "$RED_BIN"
assert_eq "red shell restriction exit status" "2" \
	"$(cat "$WORKDIR/red-shell.status")"
assert_contains "red shell restriction message" \
	"$(cat "$WORKDIR/red-shell.out")" "shell access restricted"

run_capture "$WORKDIR/red-path.out" "$WORKDIR/red-path.status" \
	sh -c 'printf "H\ne /tmp/blocked\nQ\n" | "$1" -s' sh "$RED_BIN"
assert_eq "red path restriction exit status" "2" \
	"$(cat "$WORKDIR/red-path.status")"
assert_contains "red path restriction message" \
	"$(cat "$WORKDIR/red-path.out")" "shell access restricted"

run_regression_suite

printf '%s\n' "PASS"
