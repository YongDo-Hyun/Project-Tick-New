#!/bin/sh

set -eu
export LC_ALL=C
export TZ=UTC

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LS_BIN=${LS_BIN:-"$ROOT/out/ls"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/ls-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_contains() {
	needle=$1
	haystack=$2
	case $haystack in
		*"$needle"*) ;;
		*) fail "missing '$needle'" ;;
	esac
}

assert_not_contains() {
	needle=$1
	haystack=$2
	case $haystack in
		*"$needle"*) fail "unexpected '$needle'" ;;
		*) ;;
	esac
}

assert_eq() {
	expected=$1
	actual=$2
	[ "$expected" = "$actual" ] || fail "expected '$expected' got '$actual'"
}

first_line() {
	printf '%s\n' "$1" | sed -n '1p'
}

second_line() {
	printf '%s\n' "$1" | sed -n '2p'
}

[ -x "$LS_BIN" ] || fail "missing binary: $LS_BIN"

mkdir -p "$WORKDIR/tree/subdir" "$WORKDIR/group/a_dir" "$WORKDIR/cols"
printf '%s\n' alpha > "$WORKDIR/tree/file.txt"
printf '%s\n' beta > "$WORKDIR/tree/subdir/nested.txt"
printf '%s\n' hidden > "$WORKDIR/tree/.hidden"
: > "$WORKDIR/tree/0b0000002"
: > "$WORKDIR/tree/0b0000010"
mkfifo "$WORKDIR/tree/fifo"
chmod 755 "$WORKDIR/tree/file.txt"
ln -s subdir "$WORKDIR/tree/linkdir"
: > "$WORKDIR/group/b_file"
: > "$WORKDIR/cols/aa"
: > "$WORKDIR/cols/bb"
: > "$WORKDIR/cols/cc"
: > "$WORKDIR/cols/dd"

usage_output=$("$LS_BIN" --definitely-invalid 2>&1 || true)
assert_contains "usage: ls " "$usage_output"

missing_D=$("$LS_BIN" -D 2>&1 >/dev/null || true)
assert_contains "option -D requires an argument" "$missing_D"

basic=$("$LS_BIN" -1 "$WORKDIR/tree")
assert_contains "file.txt" "$basic"
assert_contains "subdir" "$basic"
assert_not_contains ".hidden" "$basic"

default_follow=$("$LS_BIN" -1 "$WORKDIR/tree/linkdir")
assert_contains "nested.txt" "$default_follow"

with_a=$("$LS_BIN" -1a "$WORKDIR/tree")
assert_contains "." "$with_a"
assert_contains ".." "$with_a"
assert_contains ".hidden" "$with_a"

with_A=$("$LS_BIN" -1A "$WORKDIR/tree")
assert_contains ".hidden" "$with_A"
assert_not_contains "$(printf '.\n..')" "$with_A"

if [ "$(id -u)" = "0" ]; then
	implicit_A=$("$LS_BIN" -1 "$WORKDIR/tree")
	assert_contains ".hidden" "$implicit_A"
	with_I=$("$LS_BIN" -1I "$WORKDIR/tree")
	assert_not_contains ".hidden" "$with_I"
fi

escaped=$(
	cd "$WORKDIR/tree"
	touch "$(printf 'bad\013name')"
	"$LS_BIN" -1b
)
assert_contains "bad\\vname" "$escaped"

octal=$(
	cd "$WORKDIR/tree"
	"$LS_BIN" -1B
)
assert_contains "bad\\013name" "$octal"

printable=$(
	cd "$WORKDIR/tree"
	"$LS_BIN" -1q
)
assert_contains "bad?name" "$printable"

literal=$(
	cd "$WORKDIR/tree"
	"$LS_BIN" -1w
)
assert_contains "bad" "$literal"

version_sorted=$("$LS_BIN" -1v "$WORKDIR/tree/0b0000002" "$WORKDIR/tree/0b0000010")
assert_eq "$WORKDIR/tree/0b0000002" "$(first_line "$version_sorted")"
assert_eq "$WORKDIR/tree/0b0000010" "$(second_line "$version_sorted")"

classified=$("$LS_BIN" -1F "$WORKDIR/tree")
assert_contains "subdir/" "$classified"
assert_contains "linkdir@" "$classified"
assert_contains "fifo|" "$classified"
assert_contains "file.txt*" "$classified"

slash_only=$("$LS_BIN" -1p "$WORKDIR/tree")
assert_contains "subdir/" "$slash_only"
assert_not_contains "fifo|" "$slash_only"

dir_self=$("$LS_BIN" -1d "$WORKDIR/tree/subdir")
assert_eq "$WORKDIR/tree/subdir" "$dir_self"

follow_H=$("$LS_BIN" -1H "$WORKDIR/tree/linkdir")
assert_contains "nested.txt" "$follow_H"

follow_L=$("$LS_BIN" -1L "$WORKDIR/tree/linkdir")
assert_contains "nested.txt" "$follow_L"

physical_P=$("$LS_BIN" -1P "$WORKDIR/tree/linkdir")
assert_eq "$WORKDIR/tree/linkdir" "$physical_P"

physical_HP=$("$LS_BIN" -1HP "$WORKDIR/tree/linkdir")
assert_eq "$WORKDIR/tree/linkdir" "$physical_HP"

follow_PH=$("$LS_BIN" -1PH "$WORKDIR/tree/linkdir")
assert_contains "nested.txt" "$follow_PH"

long_symlink=$("$LS_BIN" -lP "$WORKDIR/tree/linkdir")
assert_contains "-> subdir" "$long_symlink"

classified_link=$("$LS_BIN" -1F "$WORKDIR/tree/linkdir")
assert_contains "@" "$classified_link"
assert_not_contains "nested.txt" "$classified_link"

recursive=$("$LS_BIN" -1R "$WORKDIR/tree")
assert_contains "$WORKDIR/tree/subdir:" "$recursive"
assert_contains "nested.txt" "$recursive"

printf 'a' > "$WORKDIR/small"
dd if=/dev/zero of="$WORKDIR/big" bs=1 count=64 >/dev/null 2>&1
size_sorted=$("$LS_BIN" -1S "$WORKDIR/small" "$WORKDIR/big")
assert_eq "$WORKDIR/big" "$(first_line "$size_sorted")"
reverse_size=$("$LS_BIN" -1Sr "$WORKDIR/small" "$WORKDIR/big")
assert_eq "$WORKDIR/small" "$(first_line "$reverse_size")"

touch -t 202401010101 "$WORKDIR/older"
touch -t 202402020202 "$WORKDIR/newer"
time_sorted=$("$LS_BIN" -1t "$WORKDIR/older" "$WORKDIR/newer")
assert_eq "$WORKDIR/newer" "$(first_line "$time_sorted")"
reverse_time=$("$LS_BIN" -1tr "$WORKDIR/older" "$WORKDIR/newer")
assert_eq "$WORKDIR/older" "$(first_line "$reverse_time")"

: > "$WORKDIR/same_a"
: > "$WORKDIR/same_b"
touch -t 202403030303 "$WORKDIR/same_a" "$WORKDIR/same_b"
reverse_ties=$("$LS_BIN" -1tr "$WORKDIR/same_a" "$WORKDIR/same_b")
assert_eq "$WORKDIR/same_b" "$(first_line "$reverse_ties")"
same_sort_flag=$("$LS_BIN" -1try "$WORKDIR/same_a" "$WORKDIR/same_b")
assert_eq "$WORKDIR/same_a" "$(first_line "$same_sort_flag")"
same_sort_env=$(LS_SAMESORT=1 "$LS_BIN" -1tr "$WORKDIR/same_a" "$WORKDIR/same_b")
assert_eq "$WORKDIR/same_a" "$(first_line "$same_sort_env")"

formatted_time=$("$LS_BIN" -lD '%s' "$WORKDIR/tree/file.txt")
assert_contains "$WORKDIR/tree/file.txt" "$formatted_time"
case $formatted_time in
	*" [0-9][0-9][0-9][0-9]"*) ;;
	*) : ;;
 esac
full_time=$("$LS_BIN" -lT "$WORKDIR/tree/file.txt")
assert_contains ":" "$full_time"

uid=$(id -u)
gid=$(id -g)
numeric_long=$("$LS_BIN" -lnD '%s' "$WORKDIR/tree/file.txt")
assert_contains " $uid " "$numeric_long"
assert_contains " $gid " "$numeric_long"

group_only=$("$LS_BIN" -lgnD '%s' "$WORKDIR/tree/file.txt")
assert_contains " $gid " "$group_only"
set -- $numeric_long
numeric_fields=$#
set -- $group_only
group_fields=$#
[ "$numeric_fields" -eq 7 ] || fail "-lnD field count mismatch"
[ "$group_fields" -eq 6 ] || fail "-lgnD field count mismatch"

block_output=$("$LS_BIN" -1s "$WORKDIR/tree/file.txt")
case $block_output in
	[0-9]*" $WORKDIR/tree/file.txt") ;;
	*) fail "-s output malformed" ;;
 esac

single_column_blocks=$("$LS_BIN" -1s "$WORKDIR/tree")
assert_not_contains "total " "$single_column_blocks"

column_blocks=$(COLUMNS=80 "$LS_BIN" -Cs "$WORKDIR/tree")
assert_contains "total " "$column_blocks"

narrow_column_blocks=$(COLUMNS=8 "$LS_BIN" -Cs "$WORKDIR/tree")
assert_not_contains "total " "$narrow_column_blocks"

stream_output=$(COLUMNS=20 "$LS_BIN" -m "$WORKDIR/tree")
assert_contains ", " "$stream_output"

columns_output=$(COLUMNS=8 "$LS_BIN" -C "$WORKDIR/cols")
assert_contains "aa  cc" "$columns_output"
assert_contains "bb  dd" "$columns_output"

across_output=$(COLUMNS=8 "$LS_BIN" -x "$WORKDIR/cols")
assert_contains "aa  bb" "$across_output"
assert_contains "cc  dd" "$across_output"

group_first=$("$LS_BIN" -1 --group-directories=first "$WORKDIR/group")
assert_eq "a_dir" "$(first_line "$group_first")"
group_last=$("$LS_BIN" -1 --group-directories=last "$WORKDIR/group")
assert_eq "b_file" "$(first_line "$group_last")"

color_always=$("$LS_BIN" --color=always -1 "$WORKDIR/group")
assert_contains "$(printf '\033[')" "$color_always"
color_never=$("$LS_BIN" --color=never -1 "$WORKDIR/group")
assert_not_contains "$(printf '\033[')" "$color_never"

color_follow=$("$LS_BIN" --color=always -1 "$WORKDIR/tree/linkdir")
assert_contains "nested.txt" "$color_follow"

u_output=$("$LS_BIN" -1U "$WORKDIR/tree/file.txt")
assert_eq "$WORKDIR/tree/file.txt" "$u_output"

missing_stderr=$(
	"$LS_BIN" "$WORKDIR/does-not-exist" 2>&1 >/dev/null || true
)
assert_contains "does-not-exist" "$missing_stderr"
assert_contains "No such file" "$missing_stderr"

root_order=$("$LS_BIN" -1 "$WORKDIR/tree/subdir" "$WORKDIR/tree/file.txt")
assert_eq "$WORKDIR/tree/file.txt" "$(first_line "$root_order")"

fast_mode=$("$LS_BIN" -1f "$WORKDIR/tree")
assert_contains ".hidden" "$fast_mode"

unsupported_o=$(
	"$LS_BIN" -o 2>&1 >/dev/null || true
)
assert_contains "option -o is not supported on Linux" "$unsupported_o"

unsupported_W=$(
	"$LS_BIN" -W 2>&1 >/dev/null || true
)
assert_contains "option -W is not supported on Linux" "$unsupported_W"

unsupported_Z=$(
	"$LS_BIN" -Z 2>&1 >/dev/null || true
)
assert_contains "option -Z is not supported on Linux" "$unsupported_Z"

bad_color=$(
	"$LS_BIN" --color=bogus 2>&1 >/dev/null || true
)
assert_contains "unsupported --color value 'bogus'" "$bad_color"

bad_group=$(
	"$LS_BIN" --group-directories=bogus 2>&1 >/dev/null || true
)
assert_contains "unsupported --group-directories value 'bogus'" "$bad_group"

printf '%s\n' "PASS"
