#!/bin/sh
# Catflap tests: each os/NAME.c supplies init_main() and is linked with the
# kernel and the shared C library into a test kernel; the programs in
# os/bin are linked against that kernel's addresses, packed into a romfs
# with os/root, and the ROM runs under msim with 256 KB of RAM.  Standard
# output and the exit status must match os/NAME.out; os/NAME.in, if there
# is one, is standard input; os/NAME.ram, if there is one, holds the RAM
# size in KB instead; os/NAME.host, if there is one, is lent as /host,
# and otherwise the tree the romfs was made from is, so that the same
# programs can be run from ROM, in place, and from /host, copied;
# os/NAME.opts, if there is one, holds more options for msim, such as
# the number of CPUs; os/NAME.sd, if there is one, is text put at the
# start of a fresh 1 MB SD card image for the run.  Every run has 4 KB
# of local memory per CPU, which the kernel needs.
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
for src in ${TESTS:-os/*.c}; do
	name=${src%.c}
	if [ "$name" = os/lua ] && [ ! -f "$OS/obj/bin/lua.a" ]; then
		continue                    # Lua is an option, and it is off
	fi
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
	for prog in "$OS"/obj/bin/*.o "$OS"/obj/bin/*.a; do
		[ -f "$prog" ] || continue
		p=$(basename "$prog"); p=${p%.[oa]}
		stack=0
		[ "$p" = lua ] && stack=16384
		if ! $MLD -f cfx -d __user_data_base -k $stack -S "$tmp.elf" -o "$tmp.root/bin/$p" "$OS/obj/lib/crt0.o" "$prog" "$OS/obj/libm.a" 2>"$tmp.err"; then
			echo "FAIL $src: cannot link program $p"
			cat "$tmp.err"
			break
		fi
	done
	base=$(( $(wc -c < "$tmp.bin") + $(wc -c < "$tmp.rl") ))
	"$OS/obj/mkromfs" -b $base "$tmp.img" "$tmp.root" > /dev/null
	cat "$tmp.bin" "$tmp.rl" "$tmp.img" > "$tmp.rom"
	if [ -f "$name.in" ]; then stdin="$name.in"; else stdin=/dev/null; fi
	if [ -f "$name.ram" ]; then ram=$(cat "$name.ram"); else ram=256; fi
	# the programs, unprelinked, are on /host unless the test brings its own
	if [ -d "$name.host" ]; then host="-H $name.host"; else host="-H $tmp.root"; fi
	if [ -f "$name.opts" ]; then opts=$(cat "$name.opts"); else opts=; fi
	if [ -f "$name.sd" ]; then
		dd if=/dev/zero of="$tmp.sd" bs=1024 count=1024 2>/dev/null
		dd if="$name.sd" of="$tmp.sd" conv=notrunc 2>/dev/null
		opts="$opts -D $tmp.sd"
	fi
	$MSIM -q -r "$tmp.rom" -m $ram -l 4 $opts $host -c 200000000 < "$stdin" > "$tmp.out" 2>"$tmp.err"
	echo "exit $?" >> "$tmp.out"
	if [ ! -f "$name.out" ]; then
		echo "NEW  $src: writing $name.out"
		cp "$tmp.out" "$name.out"
		pass=$((pass + 1))
	elif cmp -s "$tmp.out" "$name.out"; then
		pass=$((pass + 1))
	else
		echo "FAIL $src: output differs"
		diff "$name.out" "$tmp.out" | head -20
		cat "$tmp.err"
		fail=$((fail + 1))
	fi
done
rm -rf "$tmp".*
echo "Catflap tests: $pass passed, $fail failed"
[ $fail = 0 ]
