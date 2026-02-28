#!/bin/sh
#
# Copyright (c) 2026
#  Project Tick. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CHMOD_BIN=${CHMOD_BIN:-"$ROOT/out/chmod"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/chmod-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_mode() {
	expected=$1
	path=$2
	actual=$(stat -c '%a' "$path")
	[ "$actual" = "$expected" ] || fail "$path mode expected $expected got $actual"
}

[ -x "$CHMOD_BIN" ] || fail "missing binary: $CHMOD_BIN"

touch "$WORKDIR/file"
"$CHMOD_BIN" 600 "$WORKDIR/file"
assert_mode 600 "$WORKDIR/file"

"$CHMOD_BIN" u+x "$WORKDIR/file"
assert_mode 700 "$WORKDIR/file"

"$CHMOD_BIN" go-rwx "$WORKDIR/file"
assert_mode 700 "$WORKDIR/file"

"$CHMOD_BIN" a+r "$WORKDIR/file"
assert_mode 744 "$WORKDIR/file"

mkdir -p "$WORKDIR/tree/sub"
touch "$WORKDIR/tree/sub/file"
"$CHMOD_BIN" -R 755 "$WORKDIR/tree"
assert_mode 755 "$WORKDIR/tree"
assert_mode 755 "$WORKDIR/tree/sub"
assert_mode 755 "$WORKDIR/tree/sub/file"

verbose_output=$("$CHMOD_BIN" -vv 700 "$WORKDIR/file")
case $verbose_output in
	*"$WORKDIR/file: "*) ;;
	*) fail "verbose output missing file path" ;;
esac

usage_output=$("$CHMOD_BIN" 2>&1 || true)
case $usage_output in
	*"usage: chmod "* ) ;;
	*) fail "usage output missing" ;;
esac

printf '%s\n' "PASS"
