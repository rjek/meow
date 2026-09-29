#!/bin/sh
# Lua tests: each lua/NAME.lua goes to lua/lua.bin under msim on standard
# input, after a line that makes the interpreter read the rest of its input
# as one chunk, and what comes out, prompts and all, must match lua/NAME.out,
# which is what Lua 5.4 on the host prints for the same input with -i.
cd "$(dirname "$0")" || exit 2
MSIM=${MSIM:-../simulator/msim}
LUA=../lua/lua.bin
tmp=${TMPDIR:-/tmp}/luatest.$$
pass=0
fail=0
if [ ! -f "$LUA" ]; then
	echo "Lua tests skipped: no $LUA"
	exit 0
fi
for src in lua/*.lua; do
	name=${src%.lua}
	echo "assert(load(io.read('a')))()" | cat - "$src" > "$tmp.in"
	$MSIM -q -r "$LUA" -m 1024 -c 400000000 < "$tmp.in" > "$tmp.out" 2>"$tmp.err"
	echo "exit $?" >> "$tmp.out"
	if cmp -s "$tmp.out" "$name.out"; then
		pass=$((pass + 1))
	else
		echo "FAIL $src: output differs"
		diff "$name.out" "$tmp.out" | head -20
		cat "$tmp.err"
		fail=$((fail + 1))
	fi
done
rm -f "$tmp".*
echo "Lua tests: $pass passed, $fail failed"
[ $fail = 0 ]
