#!/bin/sh
# Catflap tests: each os/NAME.c supplies init_main() and is linked with the
# kernel and the shared C library into a test kernel; the programs in
# os/bin are linked against that kernel's addresses, packed into a romfs
# with os/root, and the ROM runs under msim with 256 KB of RAM.  Standard
# output and the exit status must match os/NAME.out; os/NAME.in, if there
# is one, is standard input.
cd "$(dirname "$0")" || exit 2
NMCC=${NMCC:-../../norcroft-ng/bin/nmcc}
MLD=${MLD:-../ld/mld}
MSIM=${MSIM:-../simulator/msim}
OS=../os
INC="-I../libc/pdclib/include -I../libc/meow/include -I$OS/kernel"
tmp=${TMPDIR:-/tmp}/ostest.$$
pass=0
fail=0
if [ ! -x "$NMCC" ]; then
	echo "Catflap tests skipped: no compiler at $NMCC"
	exit 0
fi
if [ ! -f "$OS/obj/link.list" ]; then
	echo "Catflap tests skipped: os/ is not built"
	exit 0
fi
LINK=$(sed "s|obj/|$OS/obj/|g; s|\.\./rt/|../rt/|g" "$OS/obj/link.list")
for src in os/*.c; do
	name=${src%.c}
	rm -rf "$tmp.root"
	if ! $NMCC -std=c99 -c $INC -o "$tmp.o" "$src" > "$tmp.err" 2>&1; then
		echo "FAIL $src: does not compile"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	if ! $MLD -f elf -B -d 0x08000000 -o "$tmp.elf" $LINK "$tmp.o" 2>"$tmp.err" ||
	   ! $MLD -f bin -p -B -d 0x08000000 -o "$tmp.bin" \
	          -R __libc_data_start,__libc_data_end,"$tmp.rl" $LINK "$tmp.o" 2>>"$tmp.err"; then
		echo "FAIL $src: does not link"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	cp -r "$OS/root/." "$tmp.root" && mkdir -p "$tmp.root/bin"
	for prog in "$OS"/obj/bin/*.o; do
		p=$(basename "$prog" .o)
		if ! $MLD -f cfx -S "$tmp.elf" -o "$tmp.root/bin/$p" "$OS/obj/lib/crt0.o" "$prog" 2>"$tmp.err"; then
			echo "FAIL $src: cannot link program $p"
			cat "$tmp.err"
			break
		fi
	done
	"$OS/obj/mkromfs" "$tmp.img" "$tmp.root" > /dev/null
	cat "$tmp.bin" "$tmp.rl" "$tmp.img" > "$tmp.rom"
	if [ -f "$name.in" ]; then stdin="$name.in"; else stdin=/dev/null; fi
	$MSIM -q -r "$tmp.rom" -m 256 -c 50000000 < "$stdin" > "$tmp.out" 2>"$tmp.err"
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
rm -rf "$tmp".*
echo "Catflap tests: $pass passed, $fail failed"
[ $fail = 0 ]
