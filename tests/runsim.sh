#!/bin/sh
# Simulator regression tests: each sim/NAME.s is assembled and run; its
# stdout plus exit status must match sim/NAME.out; sim/NAME.in, if there
# is one, is standard input.
cd "$(dirname "$0")" || exit 2
MAS=${MAS:-../as/mas}
MSIM=${MSIM:-../simulator/msim}
tmp=${TMPDIR:-/tmp}/simtest.$$
pass=0
fail=0
for src in sim/*.s; do
	name=${src%.s}
	case "$name" in *.inc) continue ;; esac
	if ! $MAS -o "$tmp.bin" "$src" 2>"$tmp.err"; then
		echo "FAIL $src: does not assemble"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	if [ -f "$name.in" ]; then stdin="$name.in"; else stdin=/dev/null; fi
	$MSIM -q -r "$tmp.bin" -c 200000 < "$stdin" > "$tmp.out" 2>"$tmp.err"
	echo "exit $?" >> "$tmp.out"
	if [ -f "$name.out" ]; then
		if cmp -s "$tmp.out" "$name.out"; then
			pass=$((pass + 1))
		else
			echo "FAIL $src: output differs"
			diff "$name.out" "$tmp.out" | head -20
			cat "$tmp.err"
			fail=$((fail + 1))
		fi
	else
		echo "NEW  $src: writing $name.out"
		cp "$tmp.out" "$name.out"
		pass=$((pass + 1))
	fi
done
rm -f "$tmp.bin" "$tmp.err" "$tmp.out"
echo "simulator tests: $pass passed, $fail failed"
[ $fail = 0 ]
