#!/bin/sh
# C tests: each c/NAME.c is compiled with nmcc -S, assembled, linked with
# the runtime in ../rt and run under msim; stdout plus the exit status must
# match c/NAME.out.  The object nmcc -c writes must link to an image that
# is identical, or at least behaves identically.  Needs the Norcroft-NG MEOW compiler: set NMCC or have
# it at ../../norcroft-ng/bin/nmcc.
cd "$(dirname "$0")" || exit 2
NMCC=${NMCC:-../../norcroft-ng/bin/nmcc}
MAS=${MAS:-../as/mas}
MLD=${MLD:-../ld/mld}
MSIM=${MSIM:-../simulator/msim}
RT="../rt/crt0.o ../rt/mul.o ../rt/div.o ../rt/ll.o ../rt/msim.o"
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
	if ! $MLD -f bin -d 0x08000000 -o "$tmp.bin" $RT "$tmp.o" 2>"$tmp.err"; then
		echo "FAIL $src: does not link"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	$MSIM -q -r "$tmp.bin" -c 50000000 > "$tmp.out" 2>"$tmp.err"
	echo "exit $?" >> "$tmp.out"
	# the compiler's own object output must give the same program
	if ! $NMCC -c -o "$tmp.2.o" -I../rt "$src" > "$tmp.err" 2>&1 ||
	   ! $MLD -f bin -d 0x08000000 -o "$tmp.2.bin" $RT "$tmp.2.o" 2>>"$tmp.err"; then
		echo "FAIL $src: object output does not compile or link"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	if ! cmp -s "$tmp.bin" "$tmp.2.bin"; then
		$MSIM -q -r "$tmp.2.bin" -c 50000000 > "$tmp.2.out" 2>"$tmp.err"
		echo "exit $?" >> "$tmp.2.out"
		if ! cmp -s "$tmp.out" "$tmp.2.out"; then
			echo "FAIL $src: object output behaves differently from -S"
			diff "$tmp.out" "$tmp.2.out" | head -20
			fail=$((fail + 1))
			continue
		fi
	fi
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
		# the host compiler is the oracle for a new test
		if cc -w -o "$tmp.host" -I../rt "$src" ../rt/msim-host.c 2>"$tmp.err"; then
			"$tmp.host" > "$name.out"
			echo "exit $?" >> "$name.out"
			rm -f "$tmp.host"
			if cmp -s "$tmp.out" "$name.out"; then
				echo "NEW  $src: matches the host, wrote $name.out"
				pass=$((pass + 1))
			else
				echo "FAIL $src: differs from the host"
				diff "$name.out" "$tmp.out" | head -20
				cp "$tmp.s" "$name.failed.s"
				rm -f "$name.out"
				fail=$((fail + 1))
			fi
		else
			echo "NEW  $src: no host compiler, wrote $name.out unchecked"
			cp "$tmp.out" "$name.out"
			pass=$((pass + 1))
		fi
	fi
done
rm -f "$tmp.s" "$tmp.o" "$tmp.bin" "$tmp.out" "$tmp.err"
echo "C tests: $pass passed, $fail failed"
[ $fail = 0 ]
