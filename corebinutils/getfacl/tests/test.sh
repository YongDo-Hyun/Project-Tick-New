#!/bin/sh
set -eu

: "${GETFACL_BIN:?GETFACL_BIN is required}"

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMPBASE="$ROOT/.tmp-tests"
mkdir -p "$TMPBASE"
WORKDIR=$(mktemp -d "$TMPBASE/getfacl-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_match() {
	pattern=$1
	text=$2
	message=$3
	printf '%s\n' "$text" | grep -Eq "$pattern" || fail "$message"
}

assert_not_match() {
	pattern=$1
	text=$2
	message=$3
	if printf '%s\n' "$text" | grep -Eq "$pattern"; then
		fail "$message"
	fi
}

assert_empty() {
	text=$1
	message=$2
	[ -z "$text" ] || fail "$message"
}

assert_eq() {
	expected=$1
	actual=$2
	message=$3
	[ "$expected" = "$actual" ] || {
		printf '%s\n' "FAIL: $message" >&2
		printf '%s\n' "--- expected ---" >&2
		printf '%s\n' "$expected" >&2
		printf '%s\n' "--- actual ---" >&2
		printf '%s\n' "$actual" >&2
		exit 1
	}
}

run_cmd() {
	: >"$STDOUT_FILE"
	: >"$STDERR_FILE"
	set +e
	"$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"
	CMD_STATUS=$?
	set -e
	CMD_STDOUT=$(cat "$STDOUT_FILE")
	CMD_STDERR=$(cat "$STDERR_FILE")
}

run_stdin_cmd() {
	input=$1
	shift
	: >"$STDOUT_FILE"
	: >"$STDERR_FILE"
	set +e
	printf '%s' "$input" | "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"
	CMD_STATUS=$?
	set -e
	CMD_STDOUT=$(cat "$STDOUT_FILE")
	CMD_STDERR=$(cat "$STDERR_FILE")
}

acl_body() {
	printf '%s\n' "$1" | sed '/^#/d;/^$/d'
}

normalize_default_body() {
	printf '%s\n' "$1" | sed 's/^default://'
}

[ -x "$GETFACL_BIN" ] || fail "missing binary: $GETFACL_BIN"

run_cmd "$GETFACL_BIN" -z
[ "$CMD_STATUS" -eq 1 ] || fail "invalid option should exit 1"
assert_match '^usage: getfacl ' "$CMD_STDERR" "usage output missing for invalid option"

touch "$WORKDIR/file"
chmod 640 "$WORKDIR/file"

run_cmd "$GETFACL_BIN" "$WORKDIR/file"
[ "$CMD_STATUS" -eq 0 ] || fail "base ACL read failed"
assert_match "^# file: $WORKDIR/file$" "$CMD_STDOUT" "missing file header"
assert_match '^# owner: ' "$CMD_STDOUT" "missing owner header"
assert_match '^# group: ' "$CMD_STDOUT" "missing group header"
assert_match '^user::rw-$' "$CMD_STDOUT" "missing user base entry"
assert_match '^group::r--$' "$CMD_STDOUT" "missing group base entry"
assert_match '^other::---$' "$CMD_STDOUT" "missing other base entry"
base_output=$CMD_STDOUT

run_cmd "$GETFACL_BIN" -h "$WORKDIR/file"
[ "$CMD_STATUS" -eq 0 ] || fail "-h should work on regular files"
assert_eq "$(acl_body "$base_output")" "$(acl_body "$CMD_STDOUT")" "-h should not change regular-file ACL body"

run_cmd "$GETFACL_BIN" -q "$WORKDIR/file"
[ "$CMD_STATUS" -eq 0 ] || fail "quiet ACL read failed"
assert_not_match '^# file:' "$CMD_STDOUT" "header was not omitted"
assert_match '^user::rw-$' "$CMD_STDOUT" "quiet output lost ACL data"

run_cmd "$GETFACL_BIN" -s -q "$WORKDIR/file"
[ "$CMD_STATUS" -eq 0 ] || fail "skip-base should not fail on trivial ACL"
assert_empty "$CMD_STDOUT" "skip-base should suppress trivial ACLs"
assert_empty "$CMD_STDERR" "skip-base should not warn for trivial ACL"

run_cmd "$GETFACL_BIN" -n "$WORKDIR/file"
[ "$CMD_STATUS" -eq 0 ] || fail "numeric ACL read failed"
assert_match "^# owner: $(id -u)$" "$CMD_STDOUT" "numeric owner header missing"
assert_match "^# group: $(id -g)$" "$CMD_STDOUT" "numeric group header missing"

run_stdin_cmd "$(printf '%s\n%s\n' "$WORKDIR/file" "$WORKDIR/file")" "$GETFACL_BIN" -q
[ "$CMD_STATUS" -eq 0 ] || fail "stdin path processing failed"
count=$(printf '%s\n' "$CMD_STDOUT" | grep -c '^user::rw-$')
[ "$count" -eq 2 ] || fail "stdin path processing did not emit two ACL blocks"
assert_match '^$' "$CMD_STDOUT" "multiple stdin paths should be separated by a blank line"

run_stdin_cmd "$(printf '%s\n\n%s\n' "$WORKDIR/file" "$WORKDIR/file")" "$GETFACL_BIN" -q
[ "$CMD_STATUS" -eq 1 ] || fail "empty stdin pathname should fail"
assert_match '^getfacl: stdin: empty pathname$' "$CMD_STDERR" "empty stdin pathname error missing"
count=$(printf '%s\n' "$CMD_STDOUT" | grep -c '^user::rw-$')
[ "$count" -eq 2 ] || fail "stdin processing should continue after empty pathname"

run_cmd "$GETFACL_BIN" "$WORKDIR/missing"
[ "$CMD_STATUS" -eq 1 ] || fail "missing file should exit 1"
assert_match "^getfacl: $WORKDIR/missing: " "$CMD_STDERR" "missing file error not reported"

run_cmd "$GETFACL_BIN" "$WORKDIR/file" "$WORKDIR/missing"
[ "$CMD_STATUS" -eq 1 ] || fail "mixed success/failure operands should exit 1"
assert_match '^user::rw-$' "$CMD_STDOUT" "successful operand output missing in mixed run"
assert_match "^getfacl: $WORKDIR/missing: " "$CMD_STDERR" "mixed operand error missing"

run_cmd "$GETFACL_BIN" -i "$WORKDIR/file"
[ "$CMD_STATUS" -eq 1 ] || fail "unsupported -i should exit 1"
assert_eq "getfacl: option -i is not supported on Linux" "$CMD_STDERR" "unsupported -i check missing"

run_cmd "$GETFACL_BIN" -v "$WORKDIR/file"
[ "$CMD_STATUS" -eq 1 ] || fail "unsupported -v should exit 1"
assert_eq "getfacl: option -v is not supported on Linux" "$CMD_STDERR" "unsupported -v check missing"

ln -s "$WORKDIR/file" "$WORKDIR/link"
run_cmd "$GETFACL_BIN" -h "$WORKDIR/link"
[ "$CMD_STATUS" -eq 1 ] || fail "symlink -h should fail on Linux"
assert_eq "getfacl: $WORKDIR/link: symbolic link ACLs are not supported on Linux" "$CMD_STDERR" "symlink -h error missing"

mkdir "$WORKDIR/plain-dir"
run_cmd "$GETFACL_BIN" -d "$WORKDIR/plain-dir"
[ "$CMD_STATUS" -eq 0 ] || fail "default ACL query without default xattr should succeed"
assert_match "^# file: $WORKDIR/plain-dir$" "$CMD_STDOUT" "default ACL header missing"
assert_not_match '^default:' "$CMD_STDOUT" "unexpected default ACL entries on plain dir"

run_cmd "$GETFACL_BIN" -d -q "$WORKDIR/plain-dir"
[ "$CMD_STATUS" -eq 0 ] || fail "quiet default ACL query without xattr should succeed"
assert_empty "$CMD_STDOUT" "quiet default ACL output without xattr should be empty"

run_cmd "$GETFACL_BIN" -d -s -q "$WORKDIR/plain-dir"
[ "$CMD_STATUS" -eq 0 ] || fail "skip-base default ACL query without xattr should succeed"
assert_empty "$CMD_STDOUT" "skip-base should suppress missing default ACL output"

acl_supported=0
if command -v setfacl >/dev/null 2>&1; then
	if setfacl -m m::r-- "$WORKDIR/file" 2>/dev/null; then
		acl_supported=1
	fi
fi

if [ "$acl_supported" -eq 1 ]; then
	run_cmd "$GETFACL_BIN" "$WORKDIR/file"
	[ "$CMD_STATUS" -eq 0 ] || fail "extended ACL read failed"
	assert_match '^mask::r--$' "$CMD_STDOUT" "extended ACL mask missing"

	setfacl -m "u:$(id -u):rw-" "$WORKDIR/file"
	setfacl -m "g:$(id -g):r--" "$WORKDIR/file"

	run_cmd "$GETFACL_BIN" -n "$WORKDIR/file"
	[ "$CMD_STATUS" -eq 0 ] || fail "numeric named ACL read failed"
	assert_match "^user:$(id -u):rw-$" "$CMD_STDOUT" "named user ACL missing"
	assert_match "^group:$(id -g):r--$" "$CMD_STDOUT" "named group ACL missing"
	assert_match '^mask::rw-$' "$CMD_STDOUT" "mask was not updated after named ACL"

	run_cmd "$GETFACL_BIN" "$WORKDIR/file"
	[ "$CMD_STATUS" -eq 0 ] || fail "named ACL read failed"
	assert_match '^user:[^:][^:]*:rw-$' "$CMD_STDOUT" "named user should resolve to a name"
	assert_match '^group:[^:][^:]*:r--$' "$CMD_STDOUT" "named group should resolve to a name"

	mkdir "$WORKDIR/dir"
	setfacl -d -m u::rwx,g::r-x,o::---,m::r-x,u:$(id -u):rwx "$WORKDIR/dir"

	run_cmd "$GETFACL_BIN" -d "$WORKDIR/dir"
	[ "$CMD_STATUS" -eq 0 ] || fail "default ACL read failed"
	assert_match '^default:user::rwx$' "$CMD_STDOUT" "default user entry missing"
	assert_match "^default:user:[^:][^:]*:rwx$" "$CMD_STDOUT" "default named user entry missing"
	assert_match '^default:group::r-x$' "$CMD_STDOUT" "default group entry missing"
	assert_match '^default:mask::r-x$' "$CMD_STDOUT" "default mask entry missing"
	assert_match '^default:other::---$' "$CMD_STDOUT" "default other entry missing"

	run_cmd "$GETFACL_BIN" -n -d "$WORKDIR/dir"
	[ "$CMD_STATUS" -eq 0 ] || fail "numeric default ACL read failed"
	assert_match "^default:user:$(id -u):rwx$" "$CMD_STDOUT" "numeric default named user entry missing"

	if command -v getfacl >/dev/null 2>&1; then
		run_cmd "$GETFACL_BIN" -n -q "$WORKDIR/file"
		ours_access=$(acl_body "$CMD_STDOUT")
		system_access=$(getfacl -E -p -n "$WORKDIR/file" 2>/dev/null | sed '/^#/d;/^$/d')
		assert_eq "$system_access" "$ours_access" "access ACL body diverges from system getfacl"

		run_cmd "$GETFACL_BIN" -n -q -d "$WORKDIR/dir"
		ours_default=$(normalize_default_body "$(acl_body "$CMD_STDOUT")")
		system_default=$(getfacl -E -p -n -d "$WORKDIR/dir" 2>/dev/null | sed '/^#/d;/^$/d')
		assert_eq "$system_default" "$ours_default" "default ACL body diverges from system getfacl"
	fi
else
	printf '%s\n' "SKIP: extended ACL checks (setfacl unavailable or filesystem lacks ACL support)" >&2
fi

printf '%s\n' "PASS"
