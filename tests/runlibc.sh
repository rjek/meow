#!/bin/sh
# C library tests: each libc/NAME.c is compiled with nmcc in C99 mode against
# PDCLib's headers, linked with the runtime and libc.a, and run under msim;
# stdout plus the exit status must match libc/NAME.out, which is what the
# host's C library gives for the same program; libc/NAME.in, if there is
# one, is standard input.
cd "$(dirname "$0")" || exit 2
NMCC=${NMCC:-../../norcroft-ng/bin/nmcc}
MLD=${MLD:-../ld/mld}
MSIM=${MSIM:-../simulator/msim}
RT="../rt/crt0.o ../rt/mul.o ../rt/div.o ../rt/ll.o ../rt/softfp.o"
LIBC=../libc/libc.a
INC="-J../libc/pdclib/include -I../libc/meow/include"
tmp=${TMPDIR:-/tmp}/libctest.$$
pass=0
fail=0
if [ ! -x "$NMCC" ]; then
	echo "C library tests skipped: no compiler at $NMCC"
	exit 0
fi
if [ ! -f "$LIBC" ]; then
	echo "C library tests skipped: no $LIBC"
	exit 0
fi
for src in libc/*.c; do
	name=${src%.c}
	if ! $NMCC -std=c99 -c $INC -o "$tmp.o" "$src" > "$tmp.err" 2>&1; then
		echo "FAIL $src: does not compile"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	if ! $MLD -f bin -d 0x08000000 -o "$tmp.bin" $RT "$tmp.o" $LIBC 2>"$tmp.err"; then
		echo "FAIL $src: does not link"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	if [ -f "$name.in" ]; then stdin="$name.in"; else stdin=/dev/null; fi
	$MSIM -q -r "$tmp.bin" -c 50000000 < "$stdin" > "$tmp.out" 2>"$tmp.err"
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
echo "C library tests: $pass passed, $fail failed"
[ $fail = 0 ]
