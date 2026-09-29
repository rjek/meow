#!/bin/sh
# Catflap tests: each os/NAME.c supplies init_main() and is linked with the
# kernel, followed in ROM by the romfs os/ builds, and msim runs it with
# 256 KB of RAM; standard output and the exit status must match os/NAME.out.
cd "$(dirname "$0")" || exit 2
NMCC=${NMCC:-../../norcroft-ng/bin/nmcc}
MLD=${MLD:-../ld/mld}
MSIM=${MSIM:-../simulator/msim}
KOBJS="../os/obj/kernel/boot.o $(ls ../os/obj/kernel/*.o | grep -v 'boot.o\|init.o\|libc_') ../os/obj/kernel/libc_start.o ../os/obj/kernel/libc_end.o ../rt/mul.o ../rt/div.o ../rt/ll.o"
ROMFS=../os/obj/romfs.img
INC="-I../libc/pdclib/include -I../libc/meow/include -I../os/kernel"
tmp=${TMPDIR:-/tmp}/ostest.$$
pass=0
fail=0
if [ ! -x "$NMCC" ]; then
	echo "Catflap tests skipped: no compiler at $NMCC"
	exit 0
fi
for src in os/*.c; do
	name=${src%.c}
	if ! $NMCC -std=c99 -c $INC -o "$tmp.o" "$src" > "$tmp.err" 2>&1; then
		echo "FAIL $src: does not compile"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	if ! $MLD -f bin -p -B -d 0x08000000 -o "$tmp.bin" -R __libc_data_start,__libc_data_end,"$tmp.rl" $KOBJS "$tmp.o" 2>"$tmp.err"; then
		echo "FAIL $src: does not link"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	cat "$tmp.bin" "$tmp.rl" "$ROMFS" > "$tmp.rom"
	if [ -f "$name.in" ]; then stdin="$name.in"; else stdin=/dev/null; fi
	$MSIM -q -r "$tmp.rom" -m 256 -c 20000000 < "$stdin" > "$tmp.out" 2>"$tmp.err"
	echo "exit $?" >> "$tmp.out"
	if cmp -s "$tmp.out" "$name.out"; then
		pass=$((pass + 1))
	else
		echo "FAIL $src: output differs"
		if [ -f "$name.out" ]; then diff "$name.out" "$tmp.out" | head -20; else cat "$tmp.out"; fi
		cat "$tmp.err"
		fail=$((fail + 1))
	fi
done
rm -f "$tmp".*
echo "Catflap tests: $pass passed, $fail failed"
[ $fail = 0 ]
