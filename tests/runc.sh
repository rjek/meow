#!/bin/sh
# C tests: each c/NAME.c is compiled with nmcc -S, assembled, linked with
# the runtime in ../rt and run under msim; stdout plus the exit status must
# match c/NAME.out.  Needs the Norcroft-NG MEOW compiler: set NMCC or have
# it at ../../norcroft-ng/bin/nmcc.
cd "$(dirname "$0")" || exit 2
NMCC=${NMCC:-../../norcroft-ng/bin/nmcc}
MAS=${MAS:-../as/mas}
MLD=${MLD:-../ld/mld}
MSIM=${MSIM:-../simulator/msim}
RT="../rt/crt0.o ../rt/mul.o ../rt/div.o ../rt/msim.o"
tmp=${TMPDIR:-/tmp}/ctest.$$
pass=0
fail=0
if [ ! -x "$NMCC" ]; then
	echo "C tests skipped: no compiler at $NMCC"
	exit 0
fi
for src in c/*.c; do
	name=${src%.c}
	if ! $NMCC -S -o "$tmp.s" -I../rt "$src" > "$tmp.err" 2>&1; then
		echo "FAIL $src: does not compile"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	if ! $MAS -f elf -o "$tmp.o" "$tmp.s" 2>"$tmp.err"; then
		echo "FAIL $src: does not assemble"
		cat "$tmp.err"
		cp "$tmp.s" "$name.failed.s"
		fail=$((fail + 1))
		continue
	fi
	if ! $MLD -f bin -o "$tmp.bin" $RT "$tmp.o" 2>"$tmp.err"; then
		echo "FAIL $src: does not link"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	$MSIM -q -r "$tmp.bin" -c 5000000 > "$tmp.out" 2>"$tmp.err"
	echo "exit $?" >> "$tmp.out"
	if [ -f "$name.out" ]; then
		if cmp -s "$tmp.out" "$name.out"; then
			pass=$((pass + 1))
		else
			echo "FAIL $src: output differs"
			diff "$name.out" "$tmp.out" | head -20
			cat "$tmp.err"
			cp "$tmp.s" "$name.failed.s"
			fail=$((fail + 1))
		fi
	else
		echo "NEW  $src: writing $name.out"
		cp "$tmp.out" "$name.out"
		pass=$((pass + 1))
	fi
done
rm -f "$tmp.s" "$tmp.o" "$tmp.bin" "$tmp.out" "$tmp.err"
echo "C tests: $pass passed, $fail failed"
[ $fail = 0 ]
